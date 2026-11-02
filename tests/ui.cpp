#define main opensense_application_main
#include "../src/main.cpp"
#undef main
#include <QTest>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QTemporaryDir config;
  if (!config.isValid()) return 4;
  QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, config.path());
  app.setOrganizationName("OpenSenseTests");
  app.setApplicationName("VisualRegression");
  app.setStyle("Fusion");
  app.setStyleSheet(kStyle);
  QSettings().setValue("appearance/reduceMotion", false);
  Window window;
  window.show();
  auto stack = window.findChild<QStackedWidget *>();
  if (!stack || stack->graphicsEffect()) return 1;
  // Exercise interrupted transitions, scroll viewports and resize, not just startup.
  for (int i = 0; i < 36; ++i) {
    window.setPage(i % 6);
    window.resize(i % 2 ? 1200 : 960, i % 2 ? 860 : 700);
    QTest::qWait(12);
    int visible = 0;
    for (int j = 0; j < stack->count(); ++j)
      if (stack->widget(j)->isVisible()) ++visible;
    if (visible != 1 || stack->graphicsEffect()) return 2;
  }
  window.setPage(0);
  window.hide(); window.show();
  window.setPage(4);
  QTest::qWait(400);
  const auto animated = stack->grab().toImage();
  QSettings().setValue("appearance/reduceMotion", true);
  for (auto w : QApplication::allWidgets()) w->update();
  QTest::qWait(50);
  const auto reduced = stack->grab().toImage();
  if (animated != reduced) {
    std::cerr << "Settled page differs with reduced motion.\n";
    return 3;
  }
  window.hide();
  for (auto w : QApplication::allWidgets())
    if (auto art = dynamic_cast<StatusArt *>(w))
      if (art->animationRunning()) return 5;
  QSettings().setValue("appearance/reduceMotion", false);
  for (auto kind : {StatusArt::Kind::Fan, StatusArt::Kind::Battery, StatusArt::Kind::Protection}) {
    StatusArt art(kind); art.resize(650,138); art.show();
    const QStringList states = kind == StatusArt::Kind::Fan ? QStringList{"eco","auto","performance","manual"}
      : kind == StatusArt::Kind::Battery ? QStringList{"Charging","Discharging","Full","Not charging",""}
      : QStringList{"on","off",""};
    for (const auto &state : states) {
      QSettings().setValue("appearance/reduceMotion", false);
      art.setState(state, state, "Teste de estado", 65);
      const auto first = art.grab().toImage();
      QTest::qWait(110);
      if (art.animationRunning() && first == art.grab().toImage()) return 6;
      QSettings().setValue("appearance/reduceMotion", true); art.syncMotion();
      if (art.animationRunning()) return 7;
      const auto still = art.grab().toImage(); QTest::qWait(70);
      if (still != art.grab().toImage()) return 8;
      art.hide(); if(art.animationRunning()) return 9; art.show();
    }
  }
  QSettings().clear();
  std::cout << "UI regression: rapid navigation, resize, hide/show and settled frames passed.\n";
  return 0;
}
