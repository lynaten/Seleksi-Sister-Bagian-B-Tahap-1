### Channel Management

- **Project and Corpus Management**

  - **Alasan**: sumber kebenaran proyek/korporus, versioning dataset, akses per-tenant.
  - **Teknologi**: Postgres (+Row Level Security), object storage (S3), signed URL; opsi DataHub/OpenMetadata untuk katalog.
  - **Alternatif**: Git-LFS untuk versi file (murah, tapi kurang cocok untuk jutaan dokumen).

- **Localization (ID/EN)**

  - **Alasan**: i18n UI + i18n model output; penting untuk go-to-market lokal/regional.
  - **Teknologi**: i18next/react-intl; translation memory sederhana di DB.
  - **Alternatif**: Managed i18n (Locize/Crowdin) → cepat, tapi ada biaya.

- **Connectors and Integrations (Email/Chat/Social/CRM)**

  - **Alasan**: kurangi _time-to-value_ pelanggan lewat integrasi siap pakai.
  - **Teknologi**: konektor per kanal (IMAP/SMTP, Slack/Teams API, Meta/X API, Zendesk/Salesforce SDK); antrean via Kafka; _connector manager_ dengan retry/backoff.
  - **Alternatif**: Zapier/Make untuk MVP (cepat, tapi vendor lock-in & biaya per task).

- **Bulk Ingestion (CSV/JSON Upload, PII Scrubbing at Ingest)**

  - **Alasan**: jalur masuk data massal + validasi + _policy enforcement_ sedini mungkin.
  - **Teknologi**: upload chunked + checksum; Pydantic/Great Expectations; DLP (Presidio) di jalur ingest.
  - **Alternatif**: Validasi di sisi klien (ringan, tapi tidak tepercaya).

- **Notification and Alerts (Email/SMS/Chat, Webhook, Rules)**

  - **Alasan**: operasional & _closed-loop actions_ (mis. trigger tiket CRM saat emosi negatif).
  - **Teknologi**: provider email/SMS (SES/SendGrid/Twilio), webhooks tersigned, rule engine ringan (OPA/rego atau DSL internal).
  - **Alternatif**: Pub/Sub ke alat pihak-ketiga (PagerDuty) untuk alert kritis.

---

### Service Delivery

- **Taxonomy Management (emotion labels)**

  - **Alasan**: _single source of truth_ label/definisi; versi label memengaruhi metrik & _ground truth_.
  - **Teknologi**: registry sederhana (Postgres) + UI kurasi; _feature flag_ untuk rollout label baru.
  - **Alternatif**: Simpan di file YAML (mudah, tapi rentan drift).

- **Confidence Calibration**

  - **Alasan**: skor kepercayaan yang terkalibrasi (Platt/Temperature scaling) → keputusan bisnis lebih akurat.
  - **Teknologi**: kalibrasi pasca-training; _evaluation pipeline_ otomatis.
  - **Alternatif**: Threshold statis → cepat, tapi bias per distribusi data.

- **Sarcasm/Irony Detector, Toxicity/Abuse**

  - **Alasan**: _edge case_ linguistik yang mengacaukan emosi; _safety_ dan moderasi kanal publik.
  - **Teknologi**: _specialized head_ atau _aux classifier_; _ensemble_ kecil di atas encoder utama.
  - **Alternatif**: Panggil API moderasi eksternal (cepat, tapi biaya & latensi).

- **Low-latency Caching**

  - **Alasan**: _hot path_ prediksi berulang (duplikasi konten) → turunkan p95.
  - **Teknologi**: Redis/KeyDB dengan key hashing konten + TTL; _request coalescing_.
  - **Alternatif**: CDN untuk hasil non-PII (terbatas untuk data privat).

- **Segmentation, Topic Clustering/Keyphrases, Report Builder**

  - **Alasan**: dari “skor emosi” jadi “insight bertindak” (segmen kanal/locale, topik, laporan).
  - **Teknologi**: embeddings + HDBSCAN/k-means; keyphrase (YAKE/Rake/KeyBERT); exporter (CSV/Parquet/PDF).
  - **Alternatif**: Hanya dashboard agregat → kurang _actionable_.

- **Human-in-the-loop (Annotation UI, Active Learning, QA)**

  - **Alasan**: _label drift_ & _domain adaptation_; _review queues_ untuk sampel berketidakpastian tinggi.
  - **Teknologi**: UI labeling, _uncertainty sampling_ (entropy/margin), metrik IAA (Cohen’s kappa), audit trail.
  - **Alternatif**: Label sekali di awal → cepat, tapi akurasi turun seiring waktu.

---

### Org Support

- **Customer Management & CRM (Account/Tenant, Support & SLA, Usage Analytics)**

  - **Alasan**: multi-tenant isolation, _entitlement_, _rate plan_; portal support & metrik SLA.
  - **Teknologi**: RLS di Postgres, _usage metering_ (gateway counter → warehouse), tiket (Zendesk/Jira).
  - **Alternatif**: Satu-tenant awal (lebih mudah), susah di-scale.

- **Billing & Monetization (Plans, Invoicing, Payments/Reconciliation)**

  - **Alasan**: arus kas & compliance finansial (retensi 7 thn, pajak).
  - **Teknologi**: Stripe/Braintree, _webhook handler_, _recon job_ periodik.
  - **Alternatif**: Manual invoice → tidak skalabel & rawan gagal tagih.

- **Legal, Risk & Compliance (Privacy, DPA, Explainability, AI Policy)**

  - **Alasan**: kepatuhan GDPR/UU PDP; dokumentasi penjelasan model; _AI use review_.
  - **Teknologi**: _records of processing_, _model cards_, _policy registry_ + approval workflow.
  - **Alternatif**: Ad-hoc dokumen → rawan _audit finding_.

- **HR & Training (Hiring, Role-based Training, Performance/OKRs)**

  - **Alasan**: _secure SDLC_ lewat pelatihan per peran (dev, data, SRE) & _skills matrix_.
  - **Teknologi**: LMS sederhana, _policy attestation_, _OKR tracker_.
  - **Alternatif**: Onboarding lisan → tacit knowledge, risiko operasional.

- **Operations & SRE (Incident, Capacity, BCDR)**

  - **Alasan**: MTTR rendah, _capacity guardrail_, pemulihan bencana (RTO/RPO).
  - **Teknologi**: runbook + on-call, autoscaling HPA, _dr-controller_ (backup+failover), _chaos drills_.
  - **Alternatif**: Backup tanpa drill → pemulihan sering gagal saat krisis.

---

### Enablers

- **App Platform & DevOps (Containers/Orchestration, API Gateway & Mesh, Observability, Release Mgmt)**

  - **Alasan**: _change velocity_ tinggi tapi aman; _progressive delivery_.
  - **Teknologi**: k8s, Kong+Istio, OTel+Prom/Grafana+ELK, Argo Rollouts.
  - **Alternatif**: Deploy manual → risiko _downtime_ saat rilis.

- **Security (IAM/RBAC, Secrets/KMS, Encryption, Vulnerability Mgmt)**

  - **Alasan**: kontrol akses _least privilege_; rahasia aman; _patch cadence_ jelas.
  - **Teknologi**: Keycloak RBAC, Vault/KMS, TLS-everywhere + AES-GCM at rest, SCA/SAST/DAST + trivy/grype.
  - **Alternatif**: Simpan secret di env → kebocoran mudah terjadi.

- **Data Governance (Policy & Stewardship, PII/PHI & DLP, Consent, Audit & Reporting)**

  - **Alasan**: data hanya dipakai sesuai consent; _audit trail_ WORM.
  - **Teknologi**: OpenMetadata+OPA, DLP service, consent ledger, S3 WORM + _scheduled reports_.
  - **Alternatif**: Consent di spreadsheet → tak bisa dibuktikan saat audit.

- **ML Platform & MLOps (Experiment/Registry, CI/CD Models, Monitoring, A/B & Shadow)**

  - **Alasan**: _reproducibility_ eksperimen, _safe rollout_ model baru.
  - **Teknologi**: MLflow/Weights\&Biases, _model CI/CD_ (build → sign → deploy), Evidently/Arize OSS, _shadow canary_.
  - **Alternatif**: Deploy manual model → _regression_ sulit dideteksi.

- **QA & Testing (Automated, Data/Model Validation, Load & Latency)**

  - **Alasan**: jaminan kualitas fungsional & non-fungsional.
  - **Teknologi**: unit/e2e (pytest/playwright), Great Expectations untuk data, locust/k6 untuk beban.
  - **Alternatif**: Hanya tes manual → _incident_ bocor ke produksi.
