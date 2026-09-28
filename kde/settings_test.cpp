#include "settingspanel.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

static bool until(const std::function<bool()> &condition) {
  QElapsedTimer timer;
  timer.start();
  while (!condition() && timer.elapsed() < 3000) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
  return condition();
}
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QTemporaryDir dir;
  if (!dir.isValid()) return 1;
  auto backend = dir.filePath("backend");
  QFile script(backend);
  if (!script.open(QIODevice::WriteOnly)) return 1;
  script.write(R"SH(#!/bin/sh
case "$1" in
config) printf '%s\n' '{"version":1,"enabled":true,"host":"192.0.2.1","port":8009,"receiver_volume":0.01,"preset":"balanced","codec":"cast-opus","target_delay_ms":100,"segment_ms":500,"bitrate_kbps":192,"media_port":17833}' ;;
status) printf '%s\n' '{"status":{"state":"idle"}}' ;;
apply)
 if [ -f "$0.fail" ]; then echo 'simulated apply failure' >&2; exit 1; fi
 cat > "$0.applied"
 cat "$0.applied"
 ;;
esac
)SH");
  script.close();
  script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  app.setProperty("shoutoutExecutable", backend);
  SettingsPanel panel;
  bool dirty = false;
  QObject::connect(&panel, &SettingsPanel::changed, &panel,
                   [&](bool value) { dirty = value; });
  panel.load();
  auto host = panel.findChild<QLineEdit *>("receiverAddress");
  auto delay = panel.findChild<QSpinBox *>("targetDelayMS");
  auto preset = panel.findChild<QComboBox *>("preset");
  auto error = panel.findChild<QLabel *>("error");
  if (!until([&] { return host->text() == "192.0.2.1:8009"; }) || dirty) return 2;
  preset->setCurrentIndex(0);
  QMetaObject::invokeMethod(preset, "activated", Qt::DirectConnection, Q_ARG(int, 0));
  if (!dirty || delay->value() != 20) return 3;
  QFile fail(backend + ".fail");
  if (!fail.open(QIODevice::WriteOnly)) return 4;
  fail.close();
  panel.save();
  if (!until([&] { return error->text().contains("simulated apply failure"); }) || !dirty) return 5;
  fail.remove();
  panel.save();
  if (!until([&] { return !dirty; })) return 6;
  QFile applied(backend + ".applied");
  if (!applied.open(QIODevice::ReadOnly)) return 7;
  auto config = QJsonDocument::fromJson(applied.readAll()).object();
  if (config["target_delay_ms"].toInt() != 20 || config["preset"] != "low-latency") return 8;
  host->setText("invalid");
  panel.save();
  if (!error->text().contains("address:port")) return 9;
  panel.defaults();
  if (!dirty || delay->value() != 100) return 10;
  panel.load();
  if (!until([&] { return !dirty && host->text() == "192.0.2.1:8009"; })) return 11;
  puts("Shared settings: load, preset, failed/successful apply, validation and reset passed.");
  return 0;
}
