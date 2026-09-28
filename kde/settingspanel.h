#pragma once
#include <QWidget>
#include <QCoreApplication>
#include <functional>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocalSocket>
#include <QProcess>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

class SettingsPanel : public QWidget {
  Q_OBJECT
public:
  explicit SettingsPanel(QWidget *parent = nullptr) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    auto description = new QLabel(
        tr("Use your desktop’s normal audio output controls for volume and mute. These "
           "settings configure where the ShoutOut device sends audio."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    enabled = new QCheckBox(tr("Enable device"), this);
    enabled->setObjectName("enabled");
    form->addRow(tr("General:"), enabled);
    devices = new QComboBox(this);
    devices->setObjectName("destination");
    devices->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    devices->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    devices->setMinimumContentsLength(30);
    form->addRow(tr("Destination:"), devices);
    host = new QLineEdit(this);
    host->setObjectName("receiverAddress");
    host->setPlaceholderText(tr("192.168.1.10:8009"));
    form->addRow(tr("Receiver address:"), host);
    scale = new QDoubleSpinBox(this);
    scale->setRange(0, 100);
    scale->setDecimals(1);
    scale->setSingleStep(1);
    scale->setSuffix(tr(" %"));
    scale->setObjectName("volumeScale");
    form->addRow(tr("Receiver volume at full desktop volume:"),
                 sliderRow(scale));
    preset = new QComboBox(this);
    preset->setObjectName("preset");
    preset->addItem(tr("Low latency"), "low-latency");
    preset->addItem(tr("Balanced"), "balanced");
    preset->addItem(tr("High quality"), "high-quality");
    preset->addItem(tr("Custom"), "custom");
    form->addRow(tr("Playback preset:"), preset);
    codec = new QComboBox(this);
    codec->setObjectName("codec");
    codec->addItem(tr("AAC live segments"), "aac-hls");
    codec->addItem(tr("Cast Streaming / Opus (experimental)"), "cast-opus");
    form->addRow(tr("Encoding / delivery:"), codec);
    segment = new QSpinBox(this);
    segment->setObjectName("segmentMS");
    segment->setRange(250, 2000);
    segment->setSingleStep(250);
    segment->setSuffix(tr(" ms"));
    form->addRow(tr("Live segment length:"), sliderRow(segment));
    delay = new QSpinBox(this);
    delay->setObjectName("targetDelayMS");
    delay->setRange(10, 1000);
    delay->setSingleStep(10);
    delay->setSuffix(tr(" ms"));
    delay->setToolTip(
        tr("Requested receiver playback delay. Actual audible delay also "
           "includes capture and encoding. Lower values can cause dropouts."));
    form->addRow(tr("Cast Streaming target delay:"), sliderRow(delay));
    bitrate = new QComboBox(this);
    bitrate->setObjectName("bitrate");
    for (int rate : {128, 192, 256, 320})
      bitrate->addItem(QString::number(rate) + tr(" kbps"), rate);
    form->addRow(tr("Encoding bitrate:"), bitrate);
    layout->addLayout(form);
    auto note =
        new QLabel(tr("The volume scale is configurable from 0–100%. Lower it "
                      "for sensitive speakers. Presets do not guarantee low "
                      "latency; receiver buffering is additional. Volume scale "
                      "applies without reconnecting. Encoding or "
                      "target-delay changes restart playback."),
                   this);
    note->setWordWrap(true);
    layout->addWidget(note);
    status = new QLabel(this);
    status->setObjectName("status");
    status->setWordWrap(true);
    layout->addWidget(status);
    error = new QLabel(this);
    error->setObjectName("error");
    error->setWordWrap(true);
    layout->addWidget(error);
    layout->addStretch();
    connect(devices, &QComboBox::activated, this, [this](int i) {
      auto d = devices->itemData(i).toJsonObject();
      if (d.isEmpty())
        return;
      host->setText(endpoint(d["host"].toString(), d["port"].toInt()));
      config["device_id"] = d["id"];
      config["device_name"] = d["name"];
      markAsChanged();
    });
    connect(host, &QLineEdit::textEdited, this, [this] {
      config["device_id"] = "";
      config["device_name"] = host->text();
      updateDevices();
      markAsChanged();
    });
    connect(scale, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this] { markAsChanged(); });
    for (auto spin : {segment, delay})
      connect(spin, qOverload<int>(&QSpinBox::valueChanged), this,
              [this] { markAsChanged(); });
    connect(enabled, &QCheckBox::toggled, this, [this] { markAsChanged(); });
    connect(codec, &QComboBox::activated, this, [this] {
      segment->parentWidget()->setEnabled(codec->currentData() == "aac-hls");
      delay->parentWidget()->setEnabled(codec->currentData() == "cast-opus");
      preset->setCurrentIndex(3);
      markAsChanged();
    });
    connect(delay, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
      if (!loading)
        preset->setCurrentIndex(3);
    });
    connect(segment, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
      if (!loading)
        preset->setCurrentIndex(3);
    });
    connect(bitrate, &QComboBox::activated, this, [this] {
      preset->setCurrentIndex(3);
      markAsChanged();
    });
    connect(preset, &QComboBox::activated, this, [this](int i) {
      loading = true;
      if (i < 3) {
        codec->setCurrentIndex(
            codec->findData(i == 2 ? "aac-hls" : "cast-opus"));
        segment->parentWidget()->setEnabled(i == 2);
        delay->parentWidget()->setEnabled(i != 2);
      }
      if (i == 0) {
        bitrate->setCurrentIndex(bitrate->findData(128));
        delay->setValue(20);
      }
      if (i == 1) {
        bitrate->setCurrentIndex(bitrate->findData(192));
        delay->setValue(100);
      }
      if (i == 2) {
        bitrate->setCurrentIndex(bitrate->findData(320));
        segment->setValue(500);
      }
      loading = false;
      markAsChanged();
    });
    timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, &SettingsPanel::refreshStatus);
    timer->start();
    deviceSocket = new QLocalSocket(this);
    connect(deviceSocket, &QLocalSocket::connected, this, [this] {
      deviceSocket->write("{\"method\":\"watch-devices\"}\n");
    });
    connect(deviceSocket, &QLocalSocket::readyRead, this, [this] {
      while (deviceSocket->canReadLine()) {
        auto doc = QJsonDocument::fromJson(deviceSocket->readLine());
        if (doc.isObject()) {
          liveDevices = doc.object()["devices"].toArray();
          updateDevices();
        }
      }
    });
    connect(deviceSocket, &QLocalSocket::disconnected, this, [this] {
      liveDevices = {};
      updateDevices();
    });
    connect(timer, &QTimer::timeout, this, &SettingsPanel::subscribeDevices);
    subscribeDevices();
  }
  ~SettingsPanel() override {
    // Disconnect before QWidget deletes controls: socket teardown emits signals.
    deviceSocket->disconnect(this);
    deviceSocket->abort();
    for (auto proc : findChildren<QProcess *>()) {
      proc->disconnect(this);
      if (proc->state() != QProcess::NotRunning) {
        proc->kill();
        proc->waitForFinished(1000);
      }
    }
  }
  void load() {
    run({"config"}, {}, [this](QByteArray bytes) {
      QJsonParseError parse;
      auto doc = QJsonDocument::fromJson(bytes, &parse);
      if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        error->setText(tr("Invalid configuration response"));
        return;
      }
      config = doc.object();
      fill();
      setNeedsSave(false);
    });
  }
  void save() {
    config["codec"] = codec->currentData().toString();
    config["target_delay_ms"] = delay->value();
    config["segment_ms"] = segment->value();
    auto address = host->text().trimmed();
    int separator = address.lastIndexOf(':');
    bool validPort = false;
    int receiverPort = address.mid(separator + 1).toInt(&validPort);
    QString receiverHost = address.left(separator);
    if (receiverHost.startsWith('[') && receiverHost.endsWith(']'))
      receiverHost = receiverHost.mid(1, receiverHost.size() - 2);
    if (separator <= 0 || receiverHost.isEmpty() || !validPort ||
        receiverPort < 1 || receiverPort > 65535) {
      error->setText(tr("Enter the receiver as address:port, for example "
                        "192.168.1.10:8009."));
      return;
    }
    config["host"] = receiverHost;
    config["port"] = receiverPort;
    config["receiver_volume"] = scale->value() / 100.0;
    config["preset"] = preset->currentData().toString();
    config["bitrate_kbps"] = bitrate->currentData().toInt();
    config["enabled"] = enabled->isChecked();
    run({"apply"}, QJsonDocument(config).toJson(QJsonDocument::Compact),
        [this](QByteArray) {
          setNeedsSave(false);
          error->setText(tr("Settings applied. Volume scale "
                            "updates without reconnecting."));
        });
  }
  void defaults() {
    scale->setValue(1);
    loading = true;
    codec->setCurrentIndex(codec->findData("cast-opus"));
    segment->parentWidget()->setEnabled(false);
    delay->parentWidget()->setEnabled(true);
    preset->setCurrentIndex(1);
    bitrate->setCurrentIndex(1);
    segment->setValue(500);
    delay->setValue(100);
    loading = false;
    enabled->setChecked(true);
    markAsChanged();
  }

signals:
  void changed(bool dirty);

private:
  void setNeedsSave(bool dirty) { emit changed(dirty); }
  void markAsChanged() { if (!loading) setNeedsSave(true); }
  QWidget *sliderRow(QDoubleSpinBox *spin) {
    auto row = new QWidget(this);
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto slider = new QSlider(Qt::Horizontal, row);
    slider->setObjectName(spin->objectName() + "Slider");
    slider->setRange(qRound(spin->minimum() * 10),
                     qRound(spin->maximum() * 10));
    slider->setSingleStep(1);
    slider->setPageStep(10);
    slider->setValue(qRound(spin->value() * 10));
    connect(slider, &QSlider::valueChanged, spin,
            [spin](int value) { spin->setValue(value / 10.0); });
    connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), slider,
            [slider](double value) { slider->setValue(qRound(value * 10)); });
    layout->addWidget(slider, 1);
    layout->addWidget(spin);
    return row;
  }
  QWidget *sliderRow(QSpinBox *spin) {
    auto row = new QWidget(this);
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto slider = new QSlider(Qt::Horizontal, row);
    slider->setObjectName(spin->objectName() + "Slider");
    slider->setRange(spin->minimum(), spin->maximum());
    slider->setSingleStep(spin->singleStep());
    slider->setPageStep(spin->singleStep() * 5);
    slider->setValue(spin->value());
    connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    connect(spin, qOverload<int>(&QSpinBox::valueChanged), slider,
            &QSlider::setValue);
    layout->addWidget(slider, 1);
    layout->addWidget(spin);
    return row;
  }
  QJsonObject config;
  QComboBox *devices, *preset, *bitrate, *codec;
  QLineEdit *host;
  QSpinBox *segment, *delay;
  QLocalSocket *deviceSocket;
  QJsonArray liveDevices;
  QDoubleSpinBox *scale;
  QCheckBox *enabled;
  QLabel *status, *error;
  QTimer *timer;
  bool loading = false, statusBusy = false;
  QString executable() const {
    const auto configured = QCoreApplication::instance()->property("shoutoutExecutable").toString();
    return configured.isEmpty() ? QStandardPaths::findExecutable("shoutout") : configured;
  }
  void run(const QStringList &args, const QByteArray &input,
           std::function<void(QByteArray)> success) {
    auto proc = new QProcess(this);
    auto deadline = new QTimer(proc);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, proc, [proc] { proc->kill(); });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError e) {
              if (e == QProcess::FailedToStart) {
                error->setText(proc->errorString());
                statusBusy = false;
                proc->deleteLater();
              }
            });
    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, proc, success](int code, QProcess::ExitStatus exit) {
              if (code == 0 && exit == QProcess::NormalExit) {
                success(proc->readAllStandardOutput());
              } else {
                error->setText(QString::fromUtf8(proc->readAllStandardError()));
              }
              statusBusy = false;
              proc->deleteLater();
            });
    proc->start(executable(), args);
    if (!input.isEmpty())
      proc->write(input);
    proc->closeWriteChannel();
    deadline->start(12000);
  }
  void fill() {
    loading = true;
    if (config["codec"].toString() == "mp3" && codec->findData("mp3") < 0)
      codec->addItem(tr("MP3 (legacy configuration)"), "mp3");
    codec->setCurrentIndex(codec->findData(config["codec"].toString()));
    segment->setValue(config["segment_ms"].toInt(500));
    delay->setValue(config["target_delay_ms"].toInt(400));
    segment->parentWidget()->setEnabled(codec->currentData() == "aac-hls");
    delay->parentWidget()->setEnabled(codec->currentData() == "cast-opus");
    host->setText(
        endpoint(config["host"].toString(), config["port"].toInt(8009)));
    scale->setValue(config["receiver_volume"].toDouble() * 100);
    int presetIndex = preset->findData(config["preset"].toString());
    preset->setCurrentIndex(presetIndex < 0 ? 3 : presetIndex);
    bitrate->setCurrentIndex(bitrate->findData(config["bitrate_kbps"].toInt()));
    enabled->setChecked(config["enabled"].toBool());
    updateDevices();
    loading = false;
  }
  QString endpoint(QString address, int port) const {
    if (address.contains(':'))
      address = "[" + address + "]";
    return address + ":" + QString::number(port);
  }
  void subscribeDevices() {
    if (deviceSocket->state() == QLocalSocket::UnconnectedState)
      deviceSocket->connectToServer(
          QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
          "/shoutout.sock");
  }
  void updateDevices() {
    QSignalBlocker blocker(devices);
    const auto selected = config["device_id"].toString();
    // Change rows in place so an open dropdown is not reset on each update.
    for (int i = devices->count() - 1; i >= 0; --i) {
      const auto id = devices->itemData(i).toJsonObject()["id"].toString();
      bool present = false;
      for (auto value : liveDevices)
        if (value.toObject()["id"].toString() == id)
          present = true;
      if (!present)
        devices->removeItem(i);
    }
    int selection = -1;
    for (auto value : liveDevices) {
      auto d = value.toObject();
      int row = -1;
      for (int i = 0; i < devices->count(); ++i)
        if (devices->itemData(i).toJsonObject()["id"] == d["id"])
          row = i;
      if (row < 0) {
        row = devices->count();
        devices->addItem(QString());
      }
      devices->setItemText(row, d["name"].toString() + " — " +
                                    d["model"].toString());
      devices->setItemData(row, d);
      if (!selected.isEmpty() && d["id"].toString() == selected)
        selection = row;
    }
    devices->setPlaceholderText(
        selected.isEmpty()
            ? tr("Choose a destination")
            : tr("%1 (unavailable)").arg(config["device_name"].toString()));
    devices->setCurrentIndex(selection);
  }
  void refreshStatus() {
    if (statusBusy)
      return;
    statusBusy = true;
    run({"status"}, {}, [this](QByteArray bytes) {
      auto s = QJsonDocument::fromJson(bytes).object()["status"].toObject();
      status->setText(
          tr("%1 · %2\nDesktop: %3%, %4 · Receiver: %5%\n%6")
              .arg(s["state"].toString(), s["player_state"].toString())
              .arg(s["sink_volume_percent"].toDouble(), 0, 'f', 0)
              .arg(s["sink_muted"].toBool() ? tr("muted") : tr("unmuted"))
              .arg(s["receiver_volume"].toDouble() * 100, 0, 'f', 1)
              .arg(s["message"].toString()));
      if (s["audio_frames"].toDouble() > 0) {
        status->setText(
            status->text() +
            tr("\nReceiver-reported buffer: %1 ms · Retransmitted packets: %2")
                .arg(s["receiver_delay_ms"].toInt())
                .arg(s["retransmits"].toDouble(), 0, 'f', 0));
      }
    });
  }
};
