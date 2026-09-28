#include "settingspanel.h"
#include <QApplication>
#include <QCloseEvent>
#include <QCommandLineParser>
#include <QDialogButtonBox>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>

class SettingsWindow : public QWidget {
public:
  SettingsWindow() {
    setWindowTitle(tr("ShoutOut"));
    setWindowIcon(QIcon::fromTheme("audio-speakers"));
    auto layout = new QVBoxLayout(this);
    auto panel = new SettingsPanel(this);
    layout->addWidget(panel);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Apply |
        QDialogButtonBox::Reset | QDialogButtonBox::RestoreDefaults |
        QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    auto apply = buttons->button(QDialogButtonBox::Apply);
    apply->setEnabled(false);
    connect(panel, &SettingsPanel::changed, this, [this, apply](bool changed) {
      dirty = changed;
      apply->setEnabled(changed);
    });
    connect(apply, &QPushButton::clicked, panel, &SettingsPanel::save);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            panel, &SettingsPanel::load);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            panel, &SettingsPanel::defaults);
    connect(buttons, &QDialogButtonBox::rejected, this, &QWidget::close);
    resize(780, 680);
    panel->load();
  }
protected:
  void closeEvent(QCloseEvent *event) override {
    if (dirty && QMessageBox::question(this, tr("Unsaved settings"),
          tr("Discard your unapplied changes?"),
          QMessageBox::Discard | QMessageBox::Cancel,
          QMessageBox::Cancel) != QMessageBox::Discard) {
      event->ignore();
      return;
    }
    event->accept();
  }
private:
  bool dirty = false;
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("ShoutOut");
  app.setDesktopFileName("shoutout");
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addOption({"backend", "Path to the ShoutOut service client", "path"});
  parser.process(app);
  app.setProperty("shoutoutExecutable", parser.value("backend"));
  SettingsWindow window;
  window.show();
  return app.exec();
}
