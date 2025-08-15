#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

// --------------------------- Utility: Timer ---------------------------
struct Timer
{
    using clock = std::chrono::high_resolution_clock;
    clock::time_point t0;
    void start() { t0 = clock::now(); }
    double stop_ms() const
    {
        auto t1 = clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
};

// --------------------------- Params & Types ---------------------------
enum class FractalType
{
    Mandelbrot,
    Julia
};

struct FractalParams
{
    int width = 1920;
    int height = 1080;
    int maxIter = 1000;
    double centerX = -0.75;
    double centerY = 0.0;
    double scale = 1.5;
    FractalType type = FractalType::Mandelbrot;
    double juliaRe = -0.8;
    double juliaIm = 0.156;
};

// --------------------------- Color Helpers & Palettes ---------------------------
enum class PaletteType
{
    Smooth,
    Original,
    Fire,
    BW,
    Gradient,
    Banded
};

static inline void hsv_to_rgb(double h, double s, double v, uint8_t &R, uint8_t &G, uint8_t &B)
{
    double H = std::fmod(std::fmod(h, 1.0) + 1.0, 1.0) * 6.0;
    int i = int(std::floor(H));
    double f = H - i, p = v * (1.0 - s), q = v * (1.0 - s * f), t = v * (1.0 - s * (1.0 - f));
    double r, g, b;
    switch ((i % 6 + 6) % 6)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
    }
    auto clamp255 = [&](double x)
    { return (uint8_t)std::clamp((int)std::lround(x * 255.0), 0, 255); };
    R = clamp255(r);
    G = clamp255(g);
    B = clamp255(b);
}

struct RGB
{
    double r, g, b;
};
static inline RGB lerp(const RGB &a, const RGB &b, double t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}
static inline void rgb_bytes(const RGB &c, uint8_t &r, uint8_t &g, uint8_t &b)
{
    r = (uint8_t)std::clamp((int)std::lround(c.r * 255.0), 0, 255);
    g = (uint8_t)std::clamp((int)std::lround(c.g * 255.0), 0, 255);
    b = (uint8_t)std::clamp((int)std::lround(c.b * 255.0), 0, 255);
}

static inline bool parse_hex_rgb(const std::string &s, RGB &out)
{
    if (s.size() != 7 || s[0] != '#')
        return false;
    auto hex = [&](char c) -> int
    {
        if ('0' <= c && c <= '9')
            return c - '0';
        if ('a' <= c && c <= 'f')
            return 10 + c - 'a';
        if ('A' <= c && c <= 'F')
            return 10 + c - 'A';
        return -1;
    };
    int r1 = hex(s[1]), r2 = hex(s[2]), g1 = hex(s[3]), g2 = hex(s[4]), b1 = hex(s[5]), b2 = hex(s[6]);
    if ((r1 | r2 | g1 | g2 | b1 | b2) < 0)
        return false;
    int R = (r1 << 4) | r2, G = (g1 << 4) | g2, B = (b1 << 4) | b2;
    out = {R / 255.0, G / 255.0, B / 255.0};
    return true;
}

struct ColorMap
{
    PaletteType type = PaletteType::Smooth;

    // global tweaks
    double hue_offset = 0.0; // add to hue (0..1 wrap)
    double sat_scale = 1.0;  // multiply saturation
    double val_scale = 1.0;  // multiply value

    // gradient / banded
    std::vector<RGB> stops;  // colors
    std::vector<int> widths; // band widths (banded)
    double feather = 1.0;    // 0..1, 1 = fully smooth

    // Built-in gradients
    static std::vector<RGB> palette_original()
    { // blue -> yellow (screenshot-like)
        return {{0.02, 0.05, 0.35}, {0.02, 0.2, 0.7}, {0.1, 0.6, 0.9}, {0.95, 0.9, 0.1}, {1.0, 1.0, 0.9}};
    }
    static std::vector<RGB> palette_fire()
    { // black -> red -> orange -> yellow -> white
        return {{0, 0, 0}, {0.3, 0.0, 0.0}, {0.8, 0.2, 0.0}, {1.0, 0.7, 0.0}, {1.0, 1.0, 0.8}};
    }

    // utility: apply HSV tweaks
    static void apply_hsv_tweaks(uint8_t &r, uint8_t &g, uint8_t &b,
                                 double hue_off, double sat_s, double val_s)
    {
        // convert to HSV (approx)
        double R = r / 255.0, G = g / 255.0, B = b / 255.0;
        double M = std::max({R, G, B}), m = std::min({R, G, B});
        double C = M - m;
        double H = 0.0;
        if (C > 1e-12)
        {
            if (M == R)
                H = std::fmod(((G - B) / C), 6.0);
            else if (M == G)
                H = ((B - R) / C) + 2.0;
            else
                H = ((R - G) / C) + 4.0;
            H /= 6.0;
            if (H < 0)
                H += 1.0;
        }
        double V = M;
        double S = (M <= 1e-12) ? 0.0 : C / M;

        H = std::fmod(H + hue_off, 1.0);
        if (H < 0)
            H += 1.0;
        S = std::clamp(S * sat_s, 0.0, 1.0);
        V = std::clamp(V * val_s, 0.0, 1.0);

        hsv_to_rgb(H, S, V, r, g, b);
    }

    // core
    void colorize(double smoothIter, int maxIter, uint8_t &r, uint8_t &g, uint8_t &b) const
    {
        if (smoothIter >= maxIter)
        {
            r = g = b = 0;
            return;
        } // inside set

        double t = smoothIter / maxIter; // 0..1
        switch (type)
        {
        case PaletteType::Smooth:
        {
            // rainbow-ish with contrast
            double tt = std::pow(t, 0.7);
            uint8_t R, G, B;
            hsv_to_rgb(std::fmod(0.95 + 10.0 * tt, 1.0), std::clamp(0.9 * sat_scale, 0.0, 1.0),
                       std::clamp(1.0 * val_scale, 0.0, 1.0), R, G, B);
            r = R;
            g = G;
            b = B;
            return;
        }
        case PaletteType::Original:
        case PaletteType::Fire:
        case PaletteType::Gradient:
        {
            const auto &pal = (type == PaletteType::Original) ? palette_original()
                              : (type == PaletteType::Fire)   ? palette_fire()
                                                              : stops;
            if (pal.empty())
            {
                r = g = b = 0;
                return;
            }
            if (pal.size() == 1)
            {
                rgb_bytes(pal[0], r, g, b);
                return;
            }
            double pos = t * (pal.size() - 1);
            int i = (int)std::floor(pos);
            double frac = pos - i;
            if (i >= (int)pal.size() - 1)
            {
                i = (int)pal.size() - 2;
                frac = 1.0;
            }
            RGB c = lerp(pal[i], pal[i + 1], frac);
            rgb_bytes(c, r, g, b);
            apply_hsv_tweaks(r, g, b, hue_offset, sat_scale, val_scale);
            return;
        }
        case PaletteType::BW:
        {
            uint8_t v = (uint8_t)std::clamp((int)std::lround(t * 255.0 * val_scale), 0, 255);
            r = g = v;
            b = v;
            return;
        }
        case PaletteType::Banded:
        {
            if (stops.empty())
            {
                r = g = b = 0;
                return;
            }
            if (stops.size() == 1)
            {
                rgb_bytes(stops[0], r, g, b);
                return;
            }
            // cycle across total width
            int n = (int)stops.size();
            std::vector<int> W = widths;
            if ((int)W.size() != n)
            {
                W.assign(n, 12);
            }
            int total = 0;
            for (int w : W)
                total += std::max(1, w);
            double tcyc = t * total;
            int acc = 0, k = 0;
            for (; k < n; ++k)
            {
                int w = std::max(1, W[k]);
                if (tcyc < acc + w)
                    break;
                acc += w;
            }
            int i = k % n, j = (i + 1) % n;
            double local = tcyc - acc;
            double w = std::max(1, W[i]);
            double alpha = std::clamp(local / w, 0.0, 1.0);
            // feather controls transition softness
            double blend = (feather <= 0) ? (alpha < 0.5 ? 0.0 : 1.0) : std::pow(alpha, 1.0 / feather);
            RGB c = lerp(stops[i], stops[j], blend);
            rgb_bytes(c, r, g, b);
            apply_hsv_tweaks(r, g, b, hue_offset, sat_scale, val_scale);
            return;
        }
        }
    }
};

// --------------------------- Fractal Generators ---------------------------
struct IFractal
{
    virtual ~IFractal() = default;
    virtual double eval_smooth(double cr, double ci, int maxIter) const = 0;
};

struct Mandelbrot : IFractal
{
    double eval_smooth(double cr, double ci, int maxIter) const override
    {
        double zr = 0.0, zi = 0.0;
        int i = 0;
        double zr2 = 0.0, zi2 = 0.0;
        for (; i < maxIter && (zr2 + zi2) <= 4.0; ++i)
        {
            zi = 2.0 * zr * zi + ci;
            zr = zr2 - zi2 + cr;
            zr2 = zr * zr;
            zi2 = zi * zi;
        }
        if (i >= maxIter)
            return (double)maxIter;
        // Smooth coloring (normalized iteration count)
        double mag = std::sqrt(zr2 + zi2);
        double nu = std::log2(std::log(mag));
        return i + 1 - nu;
    }
};

struct Julia : IFractal
{
    double cRe, cIm;
    explicit Julia(double cre, double cim) : cRe(cre), cIm(cim) {}
    double eval_smooth(double zr, double zi, int maxIter) const override
    {
        int i = 0;
        double zr2 = zr * zr, zi2 = zi * zi;
        for (; i < maxIter && (zr2 + zi2) <= 4.0; ++i)
        {
            double nzr = zr2 - zi2 + cRe;
            double nzi = 2.0 * zr * zi + cIm;
            zr = nzr;
            zi = nzi;
            zr2 = zr * zr;
            zi2 = zi * zi;
        }
        if (i >= maxIter)
            return (double)maxIter;
        double mag = std::sqrt(zr2 + zi2);
        double nu = std::log2(std::log(mag));
        return i + 1 - nu;
    }
};

// --------------------------- Image (24-bit BMP) ---------------------------
struct ImageRGB
{
    int w, h;
    std::vector<uint8_t> data; // row-major, 3 bytes/pixel (RGB)

    ImageRGB(int width, int height) : w(width), h(height), data(size_t(width) * height * 3u) {}

    uint8_t *pixel_ptr(int x, int y) { return &data[(size_t(y) * w + x) * 3u]; }

    bool save_bmp(const std::string &path) const
    {
        // BMP 24-bit, BGR, bottom-up rows, padded to 4B
        int rowSize = ((24 * w + 31) / 32) * 4;
        int imgSize = rowSize * h;
        int fileSize = 54 + imgSize;

        std::vector<uint8_t> file(size_t(fileSize), 0);

        // BITMAPFILEHEADER (14 bytes)
        file[0] = 'B';
        file[1] = 'M';
        auto put32 = [&](int offset, uint32_t v)
        {
            file[offset + 0] = (uint8_t)(v & 0xFF);
            file[offset + 1] = (uint8_t)((v >> 8) & 0xFF);
            file[offset + 2] = (uint8_t)((v >> 16) & 0xFF);
            file[offset + 3] = (uint8_t)((v >> 24) & 0xFF);
        };
        put32(2, (uint32_t)fileSize);
        put32(10, 54); // pixel data offset

        // BITMAPINFOHEADER (40 bytes)
        put32(14, 40);
        put32(18, (uint32_t)w);
        put32(22, (uint32_t)h);
        file[26] = 1;
        file[27] = 0; // planes
        file[28] = 24;
        file[29] = 0; // bpp
        put32(34, (uint32_t)imgSize);

        // Pixel data (bottom-up)
        int pad = rowSize - w * 3;
        size_t dst = 54;
        for (int y = h - 1; y >= 0; --y)
        {
            for (int x = 0; x < w; ++x)
            {
                const uint8_t *p = &data[(size_t(y) * w + x) * 3u];
                file[dst++] = p[2]; // B
                file[dst++] = p[1]; // G
                file[dst++] = p[0]; // R
            }
            for (int i = 0; i < pad; ++i)
                file[dst++] = 0;
        }

        FILE *f = std::fopen(path.c_str(), "wb");
        if (!f)
            return false;
        size_t written = std::fwrite(file.data(), 1, file.size(), f);
        std::fclose(f);
        return written == file.size();
    }
};

// --------------------------- Coordinate Mapper ---------------------------
struct PlaneMapper
{
    double xMin, xMax, yMin, yMax;
    int width, height;
    PlaneMapper(const FractalParams &p) : width(p.width), height(p.height)
    {
        double aspect = double(p.width) / double(p.height);
        xMin = p.centerX - p.scale;
        xMax = p.centerX + p.scale;
        double halfY = p.scale / aspect;
        yMin = p.centerY - halfY;
        yMax = p.centerY + halfY;
    }
    inline void pixel_to_complex(int x, int y, double &cr, double &ci) const
    {
        cr = xMin + (xMax - xMin) * (double(x) / double(width - 1));
        ci = yMax - (yMax - yMin) * (double(y) / double(height - 1)); // top->bottom
    }
};

// --------------------------- CPU Renderer ---------------------------
enum class Backend
{
    Auto,
    Omp,
    Threads,
    Serial
};

struct CPURenderer
{
    const IFractal &fractal;
    const FractalParams &params;
    const ColorMap &cmap;
    PlaneMapper mapper;

    CPURenderer(const IFractal &f, const FractalParams &p, const ColorMap &c)
        : fractal(f), params(p), cmap(c), mapper(p) {}

    void render_serial(ImageRGB &img) const
    {
        for (int y = 0; y < params.height; ++y)
        {
            for (int x = 0; x < params.width; ++x)
            {
                double cr, ci;
                mapper.pixel_to_complex(x, y, cr, ci);
                double s = (params.type == FractalType::Mandelbrot)
                               ? fractal.eval_smooth(cr, ci, params.maxIter)
                               : fractal.eval_smooth(cr, ci, params.maxIter); // same call
                uint8_t r, g, b;
                cmap.colorize(s, params.maxIter, r, g, b);
                uint8_t *p = img.pixel_ptr(x, y);
                p[0] = r;
                p[1] = g;
                p[2] = b;
            }
        }
    }

#ifdef _OPENMP
    void render_omp(ImageRGB &img, int threads) const
    {
#pragma omp parallel for schedule(dynamic, 4) num_threads(threads)
        for (int y = 0; y < params.height; ++y)
        {
            for (int x = 0; x < params.width; ++x)
            {
                double cr, ci;
                mapper.pixel_to_complex(x, y, cr, ci);
                double s = fractal.eval_smooth(cr, ci, params.maxIter);
                uint8_t r, g, b;
                cmap.colorize(s, params.maxIter, r, g, b);
                uint8_t *p = img.pixel_ptr(x, y);
                p[0] = r;
                p[1] = g;
                p[2] = b;
            }
        }
    }
#endif

    void render_threads(ImageRGB &img, int threads) const
    {
        if (threads <= 1)
        {
            render_serial(img);
            return;
        }
        threads = std::max(1, threads);
        std::vector<std::thread> pool;
        std::atomic<int> nextRow{0};
        auto worker = [&]()
        {
            int y;
            while ((y = nextRow.fetch_add(1)) < params.height)
            {
                for (int x = 0; x < params.width; ++x)
                {
                    double cr, ci;
                    mapper.pixel_to_complex(x, y, cr, ci);
                    double s = fractal.eval_smooth(cr, ci, params.maxIter);
                    uint8_t r, g, b;
                    cmap.colorize(s, params.maxIter, r, g, b);
                    uint8_t *p = img.pixel_ptr(x, y);
                    p[0] = r;
                    p[1] = g;
                    p[2] = b;
                }
            }
        };
        pool.reserve(threads);
        for (int i = 0; i < threads; ++i)
            pool.emplace_back(worker);
        for (auto &t : pool)
            t.join();
    }
};

static PaletteType parse_palette(const std::string &s)
{
    if (s == "smooth")
        return PaletteType::Smooth;
    if (s == "original")
        return PaletteType::Original;
    if (s == "fire")
        return PaletteType::Fire;
    if (s == "bw")
        return PaletteType::BW;
    if (s == "gradient")
        return PaletteType::Gradient;
    if (s == "banded")
        return PaletteType::Banded;
    return PaletteType::Smooth;
}

static void split_csv(const std::string &s, std::vector<std::string> &out)
{
    out.clear();
    size_t i = 0;
    while (i < s.size())
    {
        size_t j = s.find(',', i);
        if (j == std::string::npos)
        {
            out.push_back(s.substr(i));
            break;
        }
        out.push_back(s.substr(i, j - i));
        i = j + 1;
    }
}

// --------------------------- CLI Parsing ---------------------------
static bool parse_int(const char *s, int &out)
{
    char *e = nullptr;
    long v = strtol(s, &e, 10);
    if (!s || *e)
        return false;
    out = (int)v;
    return true;
}
static bool parse_double(const char *s, double &out)
{
    char *e = nullptr;
    double v = strtod(s, &e);
    if (!s || *e)
        return false;
    out = v;
    return true;
}

struct Options
{
    FractalParams p;
    Backend backend = Backend::Auto;
    int threads = std::max(1u, std::thread::hardware_concurrency());
    std::string out = "fractal.bmp";

    PaletteType palette = PaletteType::Smooth;
    std::vector<RGB> colors; // for gradient/banded
    std::vector<int> widths; // for banded
    double feather = 1.0;
    double hue_off = 0.0, sat = 1.0, val = 1.0;
};

static Backend parse_backend(const std::string &s)
{
    if (s == "auto")
        return Backend::Auto;
    if (s == "omp")
        return Backend::Omp;
    if (s == "threads")
        return Backend::Threads;
    if (s == "serial")
        return Backend::Serial;
    return Backend::Auto;
}

static FractalType parse_type(const std::string &s)
{
    if (s == "julia")
        return FractalType::Julia;
    return FractalType::Mandelbrot;
}

static void print_help()
{
    std::cout <<
        R"(Usage: mandelbrot [options]
  --w <int>               image width (default 1920)
  --h <int>               image height (default 1080)
  --maxiter <int>         max iterations (default 1000)
  --cx <float>            center X (default -0.75)
  --cy <float>            center Y (default 0.0)
  --scale <float>         half-width in complex plane (default 1.5)
  --type <mandelbrot|julia> (default mandelbrot)
  --jre <float>           Julia c real (default -0.8)
  --jim <float>           Julia c imag (default 0.156)
  --threads <int>         CPU threads (default HW concurrency)
  --backend <auto|omp|threads|serial> (default auto)
   --palette <smooth|original|fire|bw|gradient|banded>
  --colors  <#RRGGBB,#RRGGBB,...>  (for gradient/banded)
  --widths  <w1,w2,...>            (for banded; default all 12)
  --feather <0..1>                 (banded softness, default 1)
  --hue <float>  --sat <float>  --val <float>  (global HSV tweaks)
  --out <filename.bmp>    output BMP file (default fractal.bmp)
)";
}

static bool parse_opts(int argc, char **argv, Options &o)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto need = [&](int i)
        { return (i + 1 < argc); };
        if (a == "--help" || a == "-h")
        {
            print_help();
            return false;
        }
        else if (a == "--w" && need(i))
            parse_int(argv[++i], o.p.width);
        else if (a == "--h" && need(i))
            parse_int(argv[++i], o.p.height);
        else if (a == "--maxiter" && need(i))
            parse_int(argv[++i], o.p.maxIter);
        else if (a == "--cx" && need(i))
            parse_double(argv[++i], o.p.centerX);
        else if (a == "--cy" && need(i))
            parse_double(argv[++i], o.p.centerY);
        else if (a == "--scale" && need(i))
            parse_double(argv[++i], o.p.scale);
        else if (a == "--type" && need(i))
            o.p.type = parse_type(argv[++i]);
        else if (a == "--jre" && need(i))
            parse_double(argv[++i], o.p.juliaRe);
        else if (a == "--jim" && need(i))
            parse_double(argv[++i], o.p.juliaIm);
        else if (a == "--threads" && need(i))
            parse_int(argv[++i], o.threads);
        else if (a == "--backend" && need(i))
            o.backend = parse_backend(argv[++i]);
        else if (a == "--out" && need(i))
            o.out = argv[++i];
        else if (a == "--palette" && need(i))
            o.palette = parse_palette(argv[++i]);
        else if (a == "--colors" && need(i))
        {
            std::vector<std::string> toks;
            split_csv(argv[++i], toks);
            o.colors.clear();
            for (auto &t : toks)
            {
                RGB c;
                if (parse_hex_rgb(t, c))
                    o.colors.push_back(c);
            }
            if (o.colors.empty())
                std::cerr << "[warn] --colors parse failed; ignoring\n";
        }
        else if (a == "--widths" && need(i))
        {
            std::vector<std::string> toks;
            split_csv(argv[++i], toks);
            o.widths.clear();
            for (auto &t : toks)
            {
                int v;
                if (parse_int(t.c_str(), v))
                    o.widths.push_back(v);
            }
        }
        else if (a == "--feather" && need(i))
            parse_double(argv[++i], o.feather);
        else if (a == "--hue" && need(i))
            parse_double(argv[++i], o.hue_off);
        else if (a == "--sat" && need(i))
            parse_double(argv[++i], o.sat);
        else if (a == "--val" && need(i))
            parse_double(argv[++i], o.val);
        else
        {
            std::cerr << "Unknown or incomplete option: " << a << "\n";
            print_help();
            return false;
        }
    }
    o.p.width = std::max(1, o.p.width);
    o.p.height = std::max(1, o.p.height);
    o.p.maxIter = std::max(1, o.p.maxIter);
    o.threads = std::max(1, o.threads);
    return true;
}

// --------------------------- Main ---------------------------
int main(int argc, char **argv)
{
    Options opt;
    if (!parse_opts(argc, argv, opt))
        return 0;

    std::unique_ptr<IFractal> f;
    if (opt.p.type == FractalType::Mandelbrot)
    {
        f = std::make_unique<Mandelbrot>();
    }
    else
    {
        f = std::make_unique<Julia>(opt.p.juliaRe, opt.p.juliaIm);
    }

    ColorMap cmap;
    cmap.type = opt.palette;
    cmap.hue_offset = opt.hue_off;
    cmap.sat_scale = opt.sat;
    cmap.val_scale = opt.val;
    cmap.feather = std::clamp(opt.feather, 0.0, 1.0);

    if (opt.palette == PaletteType::Gradient)
    {
        if (!opt.colors.empty())
            cmap.stops = opt.colors;
        else
            cmap.stops = {{0, 0, 0}, {1, 1, 1}};
    }
    if (opt.palette == PaletteType::Banded)
    {
        cmap.stops = (!opt.colors.empty() ? opt.colors
                                          : std::vector<RGB>{{1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1}, {0, 0, 1}, {1, 0, 1}});
        cmap.widths = opt.widths; // may be empty -> defaulted in colorize
    }

    ImageRGB img_serial(opt.p.width, opt.p.height);
    ImageRGB img_parallel(opt.p.width, opt.p.height);

    CPURenderer renderer(*f, opt.p, cmap);

    // Serial benchmark
    Timer t;
    t.start();
    renderer.render_serial(img_serial);
    double t_serial = t.stop_ms();

    // Parallel benchmark
    double t_parallel = 0.0;
    Backend backend_used = opt.backend;

    ImageRGB *outImg = &img_serial;

    if (opt.backend == Backend::Auto)
    {
#ifdef _OPENMP
        backend_used = Backend::Omp;
#else
        backend_used = Backend::Threads;
#endif
    }

    if (backend_used == Backend::Serial)
    {
        img_parallel = img_serial; // identical output already computed
        t_parallel = t_serial;
        outImg = &img_parallel;
    }
#ifdef _OPENMP
    else if (backend_used == Backend::Omp)
    {
        t.start();
        renderer.render_omp(img_parallel, opt.threads);
        t_parallel = t.stop_ms();
        outImg = &img_parallel;
    }
#endif
    else if (backend_used == Backend::Threads)
    {
        t.start();
        renderer.render_threads(img_parallel, opt.threads);
        t_parallel = t.stop_ms();
        outImg = &img_parallel;
    }
    else
    {
        // fallback
        t_parallel = t_serial;
    }

    // Save final (parallel if used, otherwise serial)
    if (!outImg->save_bmp(opt.out))
    {
        std::cerr << "Failed to save BMP: " << opt.out << "\n";
        return 1;
    }

    // Report
    auto b2str = [](Backend b)
    {
        switch (b)
        {
        case Backend::Auto:
            return "auto";
        case Backend::Omp:
            return "OpenMP";
        case Backend::Threads:
            return "std::thread";
        case Backend::Serial:
            return "serial";
        }
        return "unknown";
    };

    std::cout << "Resolution: " << opt.p.width << "x" << opt.p.height << "\n";
    std::cout << "MaxIter:   " << opt.p.maxIter << "\n";
    std::cout << "Type:      " << (opt.p.type == FractalType::Mandelbrot ? "Mandelbrot" : "Julia") << "\n";
    if (opt.p.type == FractalType::Julia)
    {
        std::cout << "Julia c:   " << opt.p.juliaRe << " + " << opt.p.juliaIm << "i\n";
    }
    std::cout << "Backend:   " << b2str(backend_used) << "\n";
    std::cout << "Threads:   " << opt.threads << "\n";
    std::cout << "Output:    " << opt.out << "\n\n";
    std::cout << "Serial time:   " << t_serial << " ms\n";
    std::cout << "Parallel time: " << t_parallel << " ms\n";
    if (t_parallel > 0)
    {
        std::cout << "Speedup:       " << (t_serial / t_parallel) << "x\n";
    }
    return 0;
}
