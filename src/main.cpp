#include "system.hpp"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <deque>
#include <sys/stat.h>
#include <QSignalBlocker>
#include "design.hpp"
#include "status_art.hpp"
#include <QDesktopServices>
#include <QUrl>
using namespace osense;

static const char *kVersion = "0.6.0";

static QString qs(const std::string &s) { return QString::fromStdString(s); }
static QString displayValue(std::optional<double> v, QString suffix = "",
                            int decimals = 0) {
  return v ? QString::number(*v, 'f', decimals) + suffix
           : QStringLiteral("Sem leitura");
}
static bool trusted(const char *path) {
  struct stat s{};
  return !stat(path, &s) && s.st_uid == 0 && !(s.st_mode & 0022) &&
         S_ISREG(s.st_mode);
}
static QString helperPath() {
  for (auto p :
       {"/usr/local/libexec/opensense-helper", "/usr/libexec/opensense-helper"})
    if (trusted(p))
      return p;
  return {};
}
static QString coolingHelperPath() {
  for (auto p : {"/usr/local/libexec/opensense-fan-helper", "/usr/libexec/opensense-fan-helper"})
    if (trusted(p)) return p;
  return {};
}
// pkexec precisa ser o executável do sistema (root, sem escrita de grupo/outros),
// não o primeiro "pkexec" encontrado no PATH do usuário.
static QString pkexecPath() {
  for (auto p : {"/usr/bin/pkexec", "/bin/pkexec"})
    if (trusted(p))
      return p;
  return {};
}
// Valores do sysfs vêm de drivers: nunca converta double→int sem limitar antes.
static int safeInt(double v, int lo = -1000000000, int hi = 1000000000) {
  return int(std::clamp(v, double(lo), double(hi)));
}
static void setPill(QLabel *label, const QString &text, const char *tone) {
  label->setText(text);
  label->setProperty("tone", tone);
  label->style()->unpolish(label);
  label->style()->polish(label);
}

// ------------------------------------------------------------------ ícones
enum class Glyph { Overview, Cooling, Gpu, Battery, Help, About };
static QIcon glyphIcon(Glyph glyph) {
  QIcon icon;
  const std::pair<QIcon::Mode, const char *> looks[] = {
      {QIcon::Normal, "#a1a4b9"},
      {QIcon::Active, "#c5d2e0"},
      {QIcon::Selected, "#6ee7c3"}};
  for (auto look : looks) {
    QPixmap px(44, 44);
    px.setDevicePixelRatio(2);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    QColor color(look.second);
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch (glyph) {
    case Glyph::Overview:
      for (QPointF o : {QPointF(3, 3), QPointF(12, 3), QPointF(3, 12),
                        QPointF(12, 12)})
        p.drawRoundedRect(QRectF(o, QSizeF(7, 7)), 1.8, 1.8);
      break;
    case Glyph::Cooling:
      p.drawEllipse(QPointF(11, 11), 9.2, 9.2);
      p.setBrush(color);
      p.drawEllipse(QPointF(11, 11), 1.6, 1.6);
      p.setBrush(Qt::NoBrush);
      for (int angle : {0, 120, 240}) {
        p.save();
        p.translate(11, 11);
        p.rotate(angle);
        p.drawEllipse(QPointF(0, -4.6), 2.2, 3.4);
        p.restore();
      }
      break;
    case Glyph::Gpu:
      p.drawRoundedRect(QRectF(5.5, 5.5, 11, 11), 2, 2);
      for (double t : {8.5, 13.5}) {
        p.drawLine(QPointF(t, 2), QPointF(t, 5.5));
        p.drawLine(QPointF(t, 16.5), QPointF(t, 20));
        p.drawLine(QPointF(2, t), QPointF(5.5, t));
        p.drawLine(QPointF(16.5, t), QPointF(20, t));
      }
      break;
    case Glyph::Battery:
      p.drawRoundedRect(QRectF(2, 6, 16, 10), 2.4, 2.4);
      p.setBrush(color);
      p.drawRoundedRect(QRectF(4.8, 8.6, 6, 4.8), 1, 1);
      p.drawRect(QRectF(18.6, 9.2, 1.6, 3.6));
      break;
    case Glyph::About:
      p.drawEllipse(QPointF(11,7),4,4);
      p.drawArc(QRectF(3,12,16,15),0,180*16);
      break;
    case Glyph::Help: {
      p.drawEllipse(QPointF(11, 11), 9.2, 9.2);
      QFont f = p.font();
      f.setPixelSize(13);
      f.setBold(true);
      p.setFont(f);
      p.drawText(QRectF(0, 0, 22, 22), Qt::AlignCenter, "?");
      break;
    }
    }
    p.end();
    icon.addPixmap(px, look.first);
  }
  return icon;
}

// ----------------------------------------------------- gráfico e medidor
class Graph : public QWidget {
  std::deque<std::optional<double>> data;

public:
  Graph() {
    setMinimumHeight(150);
    setAccessibleName("Histórico da temperatura do processador");
  }
  void add(std::optional<double> x) {
    data.push_back(x);
    if (data.size() > 90)
      data.pop_front();
    update();
  }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int left = 34, bottom = height() - 8;
    const int top = 8, right = width() - 8;
    auto yFor = [&](double v) {
      return bottom - (bottom - top) * std::clamp(v, 0.0, 110.0) / 110.0;
    };
    p.setFont([&] {
      QFont f = font();
      f.setPixelSize(11);
      return f;
    }());
    for (int n : {0, 25, 50, 75, 100}) {
      double y = yFor(n);
      p.setPen(QColor("#1d2733"));
      p.drawLine(QPointF(left, y), QPointF(right, y));
      p.setPen(QColor("#6f8096"));
      p.drawText(QRectF(0, y - 8, left - 6, 16),
                 Qt::AlignRight | Qt::AlignVCenter, QString::number(n));
    }
    QPainterPath line;
    std::optional<QPointF> first, last;
    int i = 0;
    for (auto v : data) {
      if (v) {
        QPointF next(left + (right - left) * i / 89.0, yFor(*v));
        if (!last)
          line.moveTo(next);
        else
          line.lineTo(next);
        if (!first)
          first = next;
        last = next;
      } else
        last = {};
      ++i;
    }
    if (last && first) {
      QPainterPath area = line;
      area.lineTo(last->x(), bottom);
      area.lineTo(first->x(), bottom);
      area.closeSubpath();
      QLinearGradient g(0, top, 0, bottom);
      g.setColorAt(0, QColor(71, 215, 172, 70));
      g.setColorAt(1, QColor(71, 215, 172, 0));
      p.fillPath(area, g);
    }
    p.setPen(QPen(QColor("#6ee7c3"), 2, Qt::SolidLine, Qt::RoundCap,
                  Qt::RoundJoin));
    p.drawPath(line);
  }
};

class Gauge : public QWidget {
  QString caption;
  double maxValue;
  std::optional<double> value;
  double displayed = 0;
  QVariantAnimation sweep;

public:
  Gauge(const QString &title, double maximum) : caption(title), maxValue(maximum) {
    setMinimumSize(190, 170);
    setAccessibleName(title);
    sweep.setDuration(320);
    sweep.setEasingCurve(QEasingCurve::OutCubic);
    connect(&sweep, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
      displayed = v.toDouble(); update();
    });
    setAccessibleDescription("Sem leitura");
  }
  QSize sizeHint() const override { return {230, 190}; }
  void setValue(std::optional<double> v) {
    const auto previous = value;
    value = v;
    sweep.stop();
    if (v && previous && isVisible() && motionEnabled() && *v != displayed) {
      sweep.setStartValue(displayed); sweep.setEndValue(*v); sweep.start();
    } else displayed = v.value_or(0);
    setAccessibleDescription(v ? QString::number(safeInt(*v)) + " RPM"
                               : "Sem leitura");
    update();
  }
  void hideEvent(QHideEvent *e) override { sweep.stop(); displayed = value.value_or(0); QWidget::hideEvent(e); }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double w = width(), h = height();
    const double side = std::min(w - 24, 2 * (h - 48) / 1.5);
    QRectF r((w - side) / 2, 6, side, side);
    r.adjust(9, 9, -9, -9);
    const double arcBottom = r.center().y() + r.width() / 4;
    p.setPen(QPen(QColor("#222d3a"), 11, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 210 * 16, -240 * 16);
    if (value) {
      double fraction = std::clamp((motionEnabled() ? displayed : *value) / maxValue, 0.0, 1.0);
      if (fraction > 0) {
        p.setPen(QPen(fraction > 0.88 ? QColor("#f0b955") : QColor("#6ee7c3"),
                      11, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(r, 210 * 16, int(-240 * 16 * fraction));
      }
    }
    QFont big = font();
    big.setPixelSize(std::max(18, int(side * 0.17)));
    big.setBold(true);
    p.setFont(big);
    p.setPen(QColor("#f1f0f8"));
    p.drawText(QRectF(r.left(), r.center().y() - side * 0.16, r.width(),
                      side * 0.26),
               Qt::AlignCenter, value ? QString::number(safeInt(*value)) : "—");
    QFont small = font();
    small.setPixelSize(12);
    p.setFont(small);
    p.setPen(QColor("#a1a4b9"));
    p.drawText(QRectF(r.left(), r.center().y() + side * 0.09, r.width(), 18),
               Qt::AlignCenter, value ? "RPM" : "Sem leitura");
    QFont cap = font();
    cap.setPixelSize(13);
    p.setFont(cap);
    p.setPen(QColor("#c5d2e0"));
    p.drawText(QRectF(0, arcBottom + 12, w, 22), Qt::AlignCenter, caption);
  }
};

// ------------------------------------------------------------------ janela
class Window : public QMainWindow {
  QStackedWidget *stack = new QStackedWidget;
  QListWidget *nav = new QListWidget;
  QLabel *cpu, *ram, *battery, *gpu, *fansCard, *modeCard, *notice,
      *graphicsState, *batteryState, *healthLabel, *capacityLabel, *cyclesLabel,
      *limitState, *limitPill, *modeText, *modeNote, *manualValue, *manualHelp,
      *tempWarn, *cpuTempLabel, *pillManual, *pillProfile, *pillPower,
      *pillHelper, *backendNote, *graphicsPill;
  StatusArt *fanArt = new StatusArt(StatusArt::Kind::Fan);
  StatusArt *batteryArt = new StatusArt(StatusArt::Kind::Battery);
  StatusArt *protectionArt = new StatusArt(StatusArt::Kind::Protection);
  QProgressBar *charge = new QProgressBar;
  Graph *graph = new Graph;
  Gauge *gaugeCpu = new Gauge("Ventoinha da CPU", 8000);
  Gauge *gaugeGpu = new Gauge("Ventoinha da GPU", 8000);
  QCheckBox *queryGpu = new QCheckBox("Mostrar temperatura e uso da NVIDIA");
  QPlainTextEdit *details = new QPlainTextEdit;
  QPushButton *hybrid, *integrated, *protect, *unprotect, *applyManual;
  QSlider *slider = new QSlider(Qt::Horizontal);
  QWidget *manualPanel = nullptr;
  QButtonGroup *modeGroup = new QButtonGroup(this);
  QMap<QString, QPushButton *> modeButtons;
  QProcess *actionProc = nullptr, *gpuProc = nullptr, *envyProc = nullptr,
           *profileProc = nullptr;
  QString configuredMode, currentProfile, profileOutput, lastError, shownMode;
  QStringList availableProfiles, queue;
  QTimer timer, slowTimer;
  QSettings settings{"OpenSense", "OpenSense"};
  QJsonObject diagnostic;
  unsigned long long previousTotal = 0, previousIdle = 0;
  bool controlReady = false, manualOpen = false, closeAfter = false;
  bool manualDirty = false;

  // ---- blocos de interface ------------------------------------------------
  QVBoxLayout *page(const QString &title, const QString &subtitle,
                    Glyph glyph) {
    auto content = new QWidget;
    content->setObjectName("pageSurface");
    content->setAutoFillBackground(true);
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(20);
    layout->addWidget(new SectionHeader(title, subtitle, stack->count()));
    auto scroll = new QScrollArea;
    scroll->setObjectName("pageScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(true);
    scroll->setWidget(content);
    stack->addWidget(scroll);
    auto item = new QListWidgetItem(glyphIcon(glyph), title);
    item->setSizeHint({0, 52});
    item->setToolTip(subtitle);
    nav->addItem(item);
    return layout;
  }
  QLabel *text(const QString &value, QVBoxLayout *layout,
               const char *name = nullptr) {
    auto l = new QLabel(value);
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    if (name)
      l->setObjectName(name);
    layout->addWidget(l);
    return l;
  }
  QLabel *pill(const QString &value = "—") {
    auto l = new QLabel(value);
    l->setObjectName("pill");
    l->setProperty("tone", "off");
    l->setAlignment(Qt::AlignCenter);
    l->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return l;
  }
  QFrame *panel(const QString &title, QVBoxLayout *layout, QVBoxLayout *&inner,
                QLabel **pillOut = nullptr) {
    auto frame = new QFrame;
    frame->setObjectName("panel");
    inner = new QVBoxLayout(frame);
    inner->setContentsMargins(20, 18, 20, 20);
    inner->setSpacing(12);
    auto head = new QHBoxLayout;
    auto name = new QLabel(title);
    name->setObjectName("panelTitle");
    head->addWidget(name, 1);
    if (pillOut) {
      *pillOut = pill();
      head->addWidget(*pillOut);
    }
    inner->addLayout(head);
    layout->addWidget(frame);
    return frame;
  }
  QPushButton *button(const QString &title, QHBoxLayout *row,
                      std::function<void()> fn, bool primary = false) {
    auto b = new MotionButton(title);
    b->setMinimumHeight(42);
    if (primary)
      b->setObjectName("primary");
    row->addWidget(b);
    connect(b, &QPushButton::clicked, this, fn);
    return b;
  }
  QLabel *card(const QString &title, const QString &desc, QGridLayout *grid,
               int r, int c) {
    auto frame = new MetricFrame;
    frame->setMinimumHeight(142);
    auto v = new QVBoxLayout(frame);
    v->setContentsMargins(22, 20, 22, 18);
    v->setSpacing(9);
    auto name = new QLabel(title);
    name->setObjectName("cardTitle");
    v->addWidget(name);
    auto value = new QLabel("Lendo…");
    value->setObjectName("metric");
    value->setWordWrap(true);
    v->addWidget(value);
    auto sub = new QLabel(desc);
    sub->setWordWrap(true);
    sub->setObjectName("muted");
    v->addWidget(sub);
    v->addStretch();
    grid->addWidget(frame, r, c);
    return value;
  }

  bool busy() const { return actionProc != nullptr; }
  // Com o perfil do acer-wmi presente e o auxiliar instalado, o perfil é
  // gravado direto no firmware (com fallback e confirmação); senão, via PPD.
  bool directProfile() const {
    return acerProfile().has_value() && !helperPath().isEmpty();
  }
  void asyncRead(QString program, QStringList args, QProcess *&slot,
                 std::function<void(int, QString)> callback) {
    if (slot)
      return;
    auto p = new QProcess(this);
    slot = p;
    auto pointer = &slot;
    auto done = std::make_shared<bool>(false);
    auto finish = [p, pointer, callback, done](int code) {
      if (*done)
        return;
      *done = true;
      QString output = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
      if (code != 0)
        output = QString::fromUtf8(p->readAllStandardError()).trimmed();
      *pointer = nullptr;
      callback(code, output);
      p->deleteLater();
    };
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [finish](int code, QProcess::ExitStatus s) {
              finish(s == QProcess::NormalExit ? code : -1);
            });
    connect(p, &QProcess::errorOccurred, this,
            [finish](QProcess::ProcessError e) {
              if (e == QProcess::FailedToStart)
                finish(-1);
            });
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    p->setProcessEnvironment(env);
    p->start(program, args);
    QTimer::singleShot(5000, p, [p] {
      if (p->state() != QProcess::NotRunning)
        p->kill();
    });
  }

  // ---- modos de ventilação ---------------------------------------------
  static QString profileFor(const QString &mode) {
    return mode == "eco"           ? "power-saver"
           : mode == "performance" ? "performance"
                                   : "balanced";
  }
  QString actualMode(const FanBackend &fb, const FanState &st) const {
    if (fb.available() && st.known && !st.automatic)
      return "manual";
    if (auto ap = acerProfile()) {
      auto fromFirmware = modeFromProfile(read(ap->profile));
      if (!fromFirmware.empty())
        return qs(fromFirmware);
    }
    if (currentProfile == "power-saver")
      return "eco";
    if (currentProfile == "performance")
      return "performance";
    if (currentProfile == "balanced" || (st.known && st.automatic))
      return "auto";
    return {};
  }
  QString modeName(const QString &mode) const {
    return mode == "eco"           ? "Eco"
           : mode == "performance" ? "Desempenho"
           : mode == "manual"      ? "Manual"
           : mode == "auto"        ? "Automático"
                                   : "Não identificado";
  }
  void showMode(const QString &mode) {
    for (auto it = modeButtons.begin(); it != modeButtons.end(); ++it)
      it.value()->setChecked(it.key() == mode);
    manualPanel->setVisible(mode == "manual");
    static const QMap<QString, QString> description = {
        {"eco", "Para tarefas leves: menos ruído, consumo e calor, com "
                "desempenho reduzido. Aplica o perfil de economia de energia."},
        {"auto", "O notebook decide a rotação das ventoinhas. Aplica o perfil "
                 "equilibrado."},
        {"performance", "Para jogos e tarefas pesadas: mais potência e "
                        "ventilação, com mais ruído e consumo. Aplica o perfil "
                        "de desempenho."},
        {"manual", "Você define a velocidade das ventoinhas, de 25% a 100%. "
                   "Nesse modo o firmware pode deixar de ajustar a rotação "
                   "pela temperatura; acompanhe-a."}};
    modeText->setText(description.value(
        mode, "Os perfis de energia não estão disponíveis neste sistema."));
    if (mode == "manual" || mode.isEmpty())
      modeNote->setText("");
    else if (directProfile())
      modeNote->setText(
          QString("Ajusta potência e ventilação em conjunto. "
                  "Controle direto na sua sessão, sem repetir a senha.") +
          (mode == "performance"
               ? " Usa o perfil de desempenho mais alto aceito pelo notebook."
               : ""));
    else
      modeNote->setText(
          platformProfile().present
              ? "O perfil também é enviado ao firmware (platform_profile), que "
                "ajusta potência e ventilação por conta própria."
              : "Este notebook não expõe perfil de plataforma: o perfil só "
                "ajusta a política de energia da CPU, e a rotação continua a "
                "cargo do firmware.");
  }
  void chooseMode(const QString &mode) {
    if (busy())
      return;
    if (mode == "manual") {
      manualOpen = true;
      shownMode = "manual";
      showMode(mode);
      updateControls();
      return;
    }
    manualOpen = false;
    QStringList ops;
    auto fb = fanBackend();
    auto st = fanState(fb);
    if (fb.available() && st.known && !st.automatic)
      ops << "fan-auto";
    ops << (directProfile() ? "profile-" + mode : "profile:" + profileFor(mode));
    perform(ops);
  }
  void applyManualSpeed() {
    if (busy())
      return;
    int percent = slider->value();
    if (QMessageBox::question(
            this, "Definir velocidade manual",
            QString("Fixar as ventoinhas em %1%?\n\nNo modo manual o notebook "
                    "pode não aumentar a rotação sozinho quando esquentar. "
                    "Acompanhe a temperatura e volte ao modo Automático quando "
                    "terminar.")
                .arg(percent)) != QMessageBox::Yes)
      return;
    perform(QStringList{QString("fan-manual:%1").arg(percent)});
  }

  void graphicsText() {
    auto boot = qs(read("/proc/sys/kernel/random/boot_id"));
    bool pending = settings.value("pendingBoot").toString() == boot &&
                   !settings.value("pendingMode").toString().isEmpty();
    QString mode = configuredMode == "hybrid"       ? "Desempenho (híbrido)"
                   : configuredMode == "integrated" ? "Economia (integrado)"
                   : configuredMode.isEmpty()       ? "não identificado"
                                                    : configuredMode;
    setPill(graphicsPill, mode,
            configuredMode.isEmpty() ? "off" : (pending ? "warn" : "ok"));
    graphicsState->setText(
        pending ? "Reinicie o notebook para concluir a mudança."
                : "Esta informação descreve a configuração do EnvyControl, não "
                  "comprova qual GPU está renderizando cada aplicativo.");
  }
  void refreshServices() {
    if (busy())
      return;
    asyncRead("/usr/bin/envycontrol", {"--query"}, envyProc,
              [this](int code, QString out) {
                configuredMode = code == 0 ? out : QString();
                graphicsText();
              });
    asyncRead(
        "powerprofilesctl", {"list"}, profileProc,
        [this](int code, QString out) {
          availableProfiles.clear();
          currentProfile.clear();
          profileOutput = out;
          if (code == 0) {
            for (auto line : out.split('\n')) {
              auto clean = line.trimmed();
              bool active = clean.startsWith('*');
              if (active)
                clean = clean.mid(1).trimmed();
              for (auto key : {"power-saver", "balanced", "performance"})
                if (clean == QString(key) + ":") {
                  availableProfiles << key;
                  if (active)
                    currentProfile = key;
                }
            }
          }
          setPill(pillPower,
                  availableProfiles.isEmpty()
                      ? "Indisponível"
                      : "power-profiles-daemon · " + currentProfile,
                  availableProfiles.isEmpty() ? "bad" : "ok");
          updateControls();
          updateFanUi();
        });
  }
  void updateControls() {
    controlReady = !helperPath().isEmpty() &&
                   !pkexecPath().isEmpty() &&
                   !busy();
    setPill(pillHelper, helperPath().isEmpty() ? "Auxiliar ausente"
                                               : "Auxiliar instalado",
            helperPath().isEmpty() ? "warn" : "ok");
    bool envy = trusted("/usr/bin/envycontrol");
    hybrid->setEnabled(controlReady && envy);
    integrated->setEnabled(controlReady && envy);
    auto c = protection();
    protect->setEnabled(controlReady && !c.path.empty());
    unprotect->setEnabled(controlReady && !c.path.empty());
    auto fb = fanBackend();
    for (auto it = modeButtons.begin(); it != modeButtons.end(); ++it) {
      bool profileMode = it.key() != "manual";
      it.value()->setEnabled(
          !busy() &&
          (!profileMode || (directProfile() && controlReady && !coolingHelperPath().isEmpty()) ||
           availableProfiles.contains(profileFor(it.key()))));
    }
    bool coolingReady = controlReady && !coolingHelperPath().isEmpty();
    bool manualReady = coolingReady && fb.available();
    slider->setEnabled(manualReady);
    applyManual->setEnabled(manualReady);
    manualHelp->setVisible(!fb.available());
  }
  void updateFanUi() {
    auto fb = fanBackend();
    auto st = fanState(fb);
    auto actual = actualMode(fb, st);
    if (!manualOpen || actual == "manual")
      shownMode = actual;
    if (actual == "manual" && st.percent >= kFanMin && !manualDirty && !slider->isSliderDown()) {
      const QSignalBlocker blocker(slider);
      slider->setValue(st.percent);
    }
    showMode(shownMode);
    modeCard->setText(modeName(actual));
    fanArt->setState(actual, "Modo atual · " + modeName(actual),
      "Movimento ilustrativo. Confira a rotação real nos indicadores de RPM abaixo.");
    manualValue->setText(QString::number(slider->value()) + "%");
    auto profile = platformProfile();
    switch (fb.kind) {
    case FanKind::HwmonPwm:
      setPill(pillManual, "PWM · acer-wmi", "ok");
      break;
    case FanKind::Linuwu:
      setPill(pillManual, "Linuwu-Sense", "ok");
      break;
    default:
      setPill(pillManual, "Indisponível", "bad");
    }
    setPill(pillProfile, profile.present ? "platform_profile · " + qs(profile.driver)
                                         : "Ausente",
            profile.present ? "ok" : "off");
    if (fb.available())
      backendNote->setText(
          "O controle manual grava na interface do driver e confere o valor "
          "depois. Uma escrita aceita não prova a rotação física: confira as "
          "RPM ao lado.");
    else
      backendNote->setText(
          "Nenhuma interface de ventoinha manual (pwm) foi encontrada. As "
          "RPM e os perfis podem funcionar mesmo assim. Veja em Ajuda.");
    manualHelp->setText(
        "O controle manual está indisponível: o kernel não expôs uma interface "
        "de velocidade (pwm do acer-wmi ou Linuwu-Sense). Os modos Eco, "
        "Automático e Desempenho não dependem dela.");
  }
  void perform(const QString &operation) { perform(QStringList{operation}); }
  void perform(const QStringList &operations) {
    if (busy() || operations.isEmpty())
      return;
    for (auto op : operations)
      if (op.startsWith("gpu-")) {
        QString message =
            op == "gpu-integrated"
                ? "O modo Economia desativa a NVIDIA após reiniciar. Monitores "
                  "ligados diretamente a ela podem deixar de funcionar."
                : "O modo Desempenho habilita o uso híbrido. Jogos e "
                  "aplicativos precisam solicitar a NVIDIA para utilizá-la.";
        message += "\n\nO EnvyControl pode modificar configurações gráficas "
                   "existentes. Será necessário reiniciar; o OpenSense não "
                   "reiniciará automaticamente.\n\nAplicar a configuração?";
        if (QMessageBox::question(this, "Alterar modo gráfico", message) !=
            QMessageBox::Yes)
          return;
      }
    queue = operations;
    runNext();
  }
  void runNext() {
    QString operation = queue.takeFirst();
    bool isGpu = operation.startsWith("gpu-");
    QString program;
    QStringList args;
    if (operation.startsWith("profile:")) {
      program = "powerprofilesctl";
      args = {"set", operation.mid(8)};
    } else {
      program = pkexecPath();
      if (program.isEmpty()) {
        notice->setText("O pkexec (Polkit) não foi encontrado em um local "
                        "confiável. Instale o pacote polkit.");
        queue.clear();
        return;
      }
      const bool cooling = operation.startsWith("fan-") || operation.startsWith("profile-");
      args = {cooling ? coolingHelperPath() : helperPath(), operation};
      if (args[0].isEmpty()) {
        notice->setText(
            "Instale o auxiliar de controle para usar esta função.");
        queue.clear();
        return;
      }
    }
    auto p = new QProcess(this);
    actionProc = p;
    updateControls();
    notice->setText(
        "Aplicando alteração… Conclua a autenticação, se solicitada.");
    auto done = std::make_shared<bool>(false);
    auto finish = [this, p, operation, isGpu, done](int code) {
      if (*done)
        return;
      *done = true;
      auto out = QString::fromUtf8(p->readAllStandardOutput());
      auto error = QString::fromUtf8(p->readAllStandardError());
      actionProc = nullptr;
      p->deleteLater();
      if (code == 0) {
        if (operation.startsWith("fan-")) manualDirty = false;
        lastError.clear();
        if (isGpu) {
          settings.setValue("pendingBoot",
                            qs(read("/proc/sys/kernel/random/boot_id")));
          settings.setValue("pendingMode", operation.mid(4));
          notice->setText("Configuração salva. Reinicie quando estiver pronto "
                          "para concluir a troca de GPU.");
        } else if (operation.startsWith("battery"))
          notice->setText(
              "Proteção confirmada pelo driver. Confira o limite na aba "
              "Bateria.");
        else if (operation.startsWith("fan-"))
          notice->setText("Ventoinha: alteração confirmada. Acompanhe as RPM e "
                          "a temperatura.");
        else
          notice->setText("Perfil de energia aplicado.");
        if (!queue.isEmpty()) {
          runNext();
          return;
        }
        if (closeAfter) {
          closeAfter = false;
          close();
          return;
        }
      } else {
        queue.clear();
        closeAfter = false;
        lastError = (error + "\n" + out).trimmed().right(12000);
        notice->setText("Não foi possível confirmar a alteração. Veja os "
                        "detalhes em Ajuda.");
        QMessageBox failure(
            QMessageBox::Warning, "Alteração não confirmada",
            lastError.isEmpty()
                ? "Autenticação cancelada ou programa indisponível."
                : lastError,
            QMessageBox::Ok, this);
        failure.setTextFormat(Qt::PlainText); // saída externa nunca vira HTML
        failure.exec();
      }
      refresh();
      refreshServices();
    };
    connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [finish](int code, QProcess::ExitStatus status) {
              finish(status == QProcess::NormalExit ? code : -1);
            });
    connect(p, &QProcess::errorOccurred, this,
            [finish](QProcess::ProcessError e) {
              if (e == QProcess::FailedToStart)
                finish(-1);
            });
    p->start(program, args);
  }

  void refresh() {
    auto all = sensors();
    QString sensorText;
    std::vector<std::optional<double>> fanValues;
    for (auto &s : all) {
      sensorText += qs(s.chip + " / " + s.label) + ": " +
                    displayValue(s.value, " " + qs(s.unit)) + "\n";
      if (s.unit == "RPM")
        fanValues.push_back(s.value);
    }
    auto cpuTemp = cpuTemperature(all);
    std::ifstream stat("/proc/stat");
    std::string label, line;
    std::getline(stat, line);
    std::istringstream stream(line);
    stream >> label;
    unsigned long long val, total = 0, idle = 0;
    int i = 0;
    while (i < 8 && stream >> val) {
      total += val;
      if (i == 3 || i == 4)
        idle += val;
      ++i;
    }
    std::optional<double> use;
    if (previousTotal && total > previousTotal && idle >= previousIdle)
      use =
          100.0 * (1.0 - double(idle - previousIdle) / (total - previousTotal));
    previousTotal = total;
    previousIdle = idle;
    cpu->setText(displayValue(cpuTemp, " °C") + " · " + displayValue(use, "%"));
    cpuTempLabel->setText("CPU " + displayValue(cpuTemp, " °C"));
    graph->add(cpuTemp);
    std::ifstream mem("/proc/meminfo");
    unsigned long long mt = 0, ma = 0;
    while (std::getline(mem, line)) {
      std::istringstream m(line);
      m >> label >> val;
      if (label == "MemTotal:")
        mt = val;
      if (label == "MemAvailable:")
        ma = val;
    }
    ram->setText(mt ? QString::number(double(mt - ma) / 1048576, 'f', 1) +
                          " / " +
                          QString::number(double(mt) / 1048576, 'f', 1) + " GiB"
                    : "Sem leitura");

    gaugeCpu->setValue(fanValues.size() > 0 ? fanValues[0] : std::nullopt);
    gaugeGpu->setValue(fanValues.size() > 1 ? fanValues[1] : std::nullopt);
    if (fanValues.empty())
      fansCard->setText("Sem leitura");
    else {
      QStringList parts;
      for (size_t n = 0; n < fanValues.size(); ++n)
        parts << (n == 0 ? "CPU " : n == 1 ? "GPU " : "Fan " +
                                                          QString::number(n + 1) +
                                                          " ") +
                     displayValue(fanValues[n]);
      fansCard->setText(parts.join(" · ") + " RPM");
    }

    auto bats = batteries();
    QString batteryDump;
    auto c = protection();
    QString limit;
    QString protectionMode;
    const char *limitTone = "off";
    if (c.path.empty())
      limit = "Controle de carga não disponível. Consulte Ajuda para verificar "
              "o suporte.";
    else {
      auto v = read(c.path);
      bool on = c.standard ? v == "80" : v == "1";
      bool off = c.standard ? v == "100" : v == "0";
      limit = on    ? "Proteção ativada · limite de 80%"
              : off ? (c.standard ? "Proteção desativada · carga até 100%"
                                  : "Proteção desativada · carga completa "
                                    "permitida")
                    : (c.standard ? "Limite informado: " + qs(v) + "%"
                                  : "Não foi possível ler o estado da proteção.");
      limitTone = on ? "ok" : off ? "off" : "warn";
      protectionMode = on ? "on" : off ? "off" : "";
    }
    limitState->setText(limit);
    protectionArt->setState(protectionMode, protectionMode == "on" ? "Proteção de 80%" : protectionMode == "off" ? "Carga completa liberada" : "Proteção sem confirmação",
      limit, protectionMode == "on" ? 80 : protectionMode == "off" ? 100 : -1);
    setPill(limitPill, c.path.empty() ? "Indisponível"
                                      : (std::string(limitTone) == "ok"
                                             ? "Ativada"
                                             : std::string(limitTone) == "off"
                                                   ? "Desativada"
                                                   : "Atenção"),
            c.path.empty() ? "bad" : limitTone);
    if (bats.empty()) {
      battery->setText("Não detectada");
      batteryArt->setState("", "Bateria não detectada", "Aguardando uma leitura válida do sistema.");
      charge->setValue(0);
      charge->setFormat("Sem leitura");
      batteryState->setText("O sistema não informou uma bateria interna.");
      healthLabel->setText("Saúde estimada: sem leitura");
      capacityLabel->clear();
      cyclesLabel->clear();
    } else {
      auto b = bats[0];
      auto percent = number(read(b / "capacity"));
      battery->setText(displayValue(percent, "%"));
      charge->setValue(percent ? safeInt(*percent, 0, 100) : 0);
      charge->setFormat(percent ? "%p%" : "Sem leitura");
      QString status = qs(read(b / "status"));
      const QString rawStatus = status;
      if (status == "Charging")
        status = "Carregando";
      else if (status == "Discharging")
        status = "Usando bateria";
      else if (status == "Full")
        status = "Carga completa";
      else if (status == "Not charging")
        status = "Conectada, sem carregar";
      batteryArt->setState(rawStatus, status.isEmpty() ? "Estado desconhecido" : status,
        percent ? "Carga atual: " + QString::number(safeInt(*percent, 0, 100)) + "% · leitura do sistema" : "Percentual de carga indisponível", percent ? safeInt(*percent, 0, 100) : -1);
      batteryState->setText(
          qs(b.filename().string()) + " · " + status +
          (bats.size() > 1
               ? " · múltiplas baterias; controle de carga desativado"
               : ""));
      auto h = health(b);
      healthLabel->setText(
          h ? "Saúde estimada: " + QString::number(h->percent, 'f', 1) + "%"
            : "Saúde estimada: sem leitura");
      capacityLabel->setText(
          h ? "Capacidade atual: " + QString::number(h->full, 'f', 2) + " " +
                  qs(h->unit) + "  /  de fábrica: " +
                  QString::number(h->design, 'f', 2) + " " + qs(h->unit)
            : "O driver não informou capacidades comparáveis.");
      cyclesLabel->setText("Ciclos de carga: " +
                           displayValue(number(read(b / "cycle_count"))));
      batteryDump = batteryState->text() + "\n" + healthLabel->text() + "\n" +
                    capacityLabel->text() + "\n" + cyclesLabel->text();
    }

    auto fb = fanBackend();
    auto st = fanState(fb);
    bool hot = fb.available() && st.known && !st.automatic && cpuTemp &&
               *cpuTemp >= 85;
    tempWarn->setVisible(hot);
    if (hot)
      tempWarn->setText("A CPU está a " + displayValue(cpuTemp, " °C") +
                        " com a ventoinha em modo manual. Volte ao modo "
                        "Automático ou aumente a velocidade.");
    updateControls();
    updateFanUi();

    auto profile = platformProfile();
    QString fanReport =
        fb.kind == FanKind::HwmonPwm ? "PWM (hwmon acer)"
        : fb.kind == FanKind::Linuwu ? "Linuwu-Sense (fan_speed)"
                                     : "indisponível";
    QString diagnostics =
        QString("OpenSense ") + kVersion + "\nModelo: " +
        qs(read("/sys/class/dmi/id/product_name")) +
        "\nKernel: " + qs(read("/proc/sys/kernel/osrelease")) + "\nAuxiliar: " +
        (helperPath().isEmpty() ? "não instalado" : helperPath()) +
        "\nEnvyControl: " + configuredMode +
        "\nControle de ventoinhas: " + fanReport + "\nPerfil de plataforma: " +
        (profile.present ? qs(profile.driver) + " (" + qs(profile.choices) + ")"
                         : QString("ausente")) +
        "\n\n" + batteryDump + "\n" + limit + "\n\n" + sensorText +
        "\nPerfis de energia:\n" + profileOutput + "\nÚltimo erro:\n" +
        lastError;
    details->setPlainText(diagnostics);
    diagnostic = {{"version", kVersion}, {"report", diagnostics}};
    if (queryGpu->isChecked())
      asyncRead(
          "nvidia-smi",
          {"--query-gpu=temperature.gpu,utilization.gpu",
           "--format=csv,noheader,nounits"},
          gpuProc, [this](int code, QString out) {
            if (!queryGpu->isChecked())
              return;
            auto fields = out.section('\n', 0, 0).split(',');
            gpu->setText(
                code == 0 && fields.size() == 2
                    ? displayValue(number(fields[0].trimmed().toStdString()),
                                   " °C") +
                          " · " +
                          displayValue(
                              number(fields[1].trimmed().toStdString()), "%")
                    : "Sem leitura");
          });
    else
      gpu->setText("Consulta pausada");
  }

public:
  Window() {
    setWindowTitle("OpenSense");
    setWindowIcon(shurikenIcon());
    resize(1200, 860);
    setMinimumSize(900, 640);
    auto root = new QWidget;
    root->setObjectName("root");
    auto outer = new QHBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    setCentralWidget(root);

    // Barra lateral
    auto side = new QFrame;
    side->setObjectName("sidebar");
    side->setFixedWidth(238);
    auto sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 22, 0, 18);
    sideLayout->setSpacing(6);
    auto brandRow = new QHBoxLayout;
    brandRow->setContentsMargins(22, 0, 16, 10);
    auto logo = new ShurikenMark;
    brandRow->addWidget(logo);
    auto brand = new QLabel("OpenSense");
    brand->setObjectName("brand");
    brandRow->addWidget(brand, 1);
    sideLayout->addLayout(brandRow);
    auto section = new QLabel("CENTRAL DE CONTROLE");
    section->setObjectName("navSection");
    sideLayout->addWidget(section);
    nav->setObjectName("nav");
    nav->setFocusPolicy(Qt::StrongFocus);
    nav->setIconSize({22, 22});
    nav->setAccessibleName("Seções do aplicativo");
    nav->setFrameShape(QFrame::NoFrame);
    sideLayout->addWidget(nav, 1);
    pillHelper = pill();
    auto footer = new QVBoxLayout;
    footer->setContentsMargins(22, 0, 22, 0);
    footer->setSpacing(8);
    auto device = new QLabel(qs(read("/sys/class/dmi/id/product_name")));
    device->setTextFormat(Qt::PlainText);
    device->setWordWrap(true);
    device->setObjectName("deviceName");
    footer->addWidget(device);
    footer->addWidget(pillHelper);
    auto version = new QLabel(QString("Versão ") + kVersion + " · beta");
    version->setObjectName("muted");
    footer->addWidget(version);
    auto reduceMotion = new QCheckBox("Reduzir animações");
    reduceMotion->setChecked(!motionEnabled());
    reduceMotion->setToolTip("Desativa transições e movimentos decorativos.");
    footer->addWidget(reduceMotion);
    connect(reduceMotion, &QCheckBox::toggled, this, [](bool checked) {
      QSettings().setValue("appearance/reduceMotion", checked);
      for (auto widget : QApplication::allWidgets()) {
        if (auto art = dynamic_cast<StatusArt *>(widget)) art->syncMotion();
        widget->update();
      }
    });
    sideLayout->addLayout(footer);
    outer->addWidget(side);

    // Conteúdo
    auto right = new QWidget;
    auto rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    auto topbar = new QFrame;
    topbar->setObjectName("topbar");
    auto topRow = new QHBoxLayout(topbar);
    topRow->setContentsMargins(28, 12, 28, 12);
    auto product = new QLabel("HARDWARE CONTROL"); product->setObjectName("eyebrow");
    topRow->addWidget(product); topRow->addStretch();
    auto platform = new QLabel("LINUX  /  DARK EDITION"); platform->setObjectName("topMeta");
    topRow->addWidget(platform);
    rightLayout->addWidget(topbar);
    rightLayout->addWidget(stack, 1);
    notice = new QLabel(
        "Monitoramento ativo. Os controles dependem do suporte do notebook.");
    notice->setObjectName("notice");
    notice->setWordWrap(true);
    notice->setTextFormat(Qt::PlainText);
    rightLayout->addWidget(notice);
    outer->addWidget(right, 1);

    QVBoxLayout *v, *inner;
    QHBoxLayout *row;

    // ---- Visão geral
    v = page("Visão geral", "Estado do notebook em tempo real.",
             Glyph::Overview);
    auto grid = new QGridLayout;
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);
    for (int c = 0; c < 3; ++c)
      grid->setColumnStretch(c, 1);
    cpu = card("Processador", "Temperatura e uso atual", grid, 0, 0);
    ram = card("Memória", "Utilizada / total", grid, 0, 1);
    battery = card("Bateria", "Carga disponível agora", grid, 0, 2);
    fansCard = card("Ventoinhas", "Rotação informada pelos sensores", grid, 1, 0);
    modeCard = card("Modo de ventilação", "Veja e altere em Resfriamento", grid,
                    1, 1);
    gpu = card("NVIDIA", "Temperatura e uso da primeira GPU", grid, 1, 2);
    v->addLayout(grid);
    auto shortcuts = new QHBoxLayout;
    button("Ajustar resfriamento  →", shortcuts, [this] { setPage(1); }, true);
    button("Proteção da bateria  →", shortcuts, [this] { setPage(3); });
    shortcuts->addStretch();
    v->addLayout(shortcuts);
    panel("Temperatura do processador · últimas 90 leituras", v, inner);
    inner->addWidget(graph);
    inner->addWidget(queryGpu);
    text("Consultar a NVIDIA pode acordar a placa e consumir mais energia. "
         "Deixe pausado quando não precisar.",
         inner, "muted");
    connect(queryGpu, &QCheckBox::toggled, this, [this] { refresh(); });
    v->addStretch();

    // ---- Resfriamento
    v = page("Resfriamento",
             "Escolha como o notebook equilibra desempenho, ruído e calor.",
             Glyph::Cooling);
    panel("Modo de ventilação", v, inner);
    auto segments = new QFrame;
    segments->setObjectName("segments");
    auto segLayout = new QHBoxLayout(segments);
    segLayout->setContentsMargins(5, 5, 5, 5);
    segLayout->setSpacing(4);
    for (auto pair : {std::make_pair("eco", "Eco"),
                      std::make_pair("auto", "Automático"),
                      std::make_pair("performance", "Desempenho"),
                      std::make_pair("manual", "Manual")}) {
      QString key = pair.first;
      auto b = new MotionButton(pair.second);
      b->setIcon(modeSymbolIcon(key));
      b->setIconSize(QSize(26,26));
      b->setObjectName("segment");
      b->setCheckable(true);
      b->setMinimumHeight(62);
      b->setAccessibleName(QString("Modo ") + pair.second);
      modeGroup->addButton(b);
      modeButtons[key] = b;
      segLayout->addWidget(b, 1);
      connect(b, &QPushButton::clicked, this, [this, key] { chooseMode(key); });
    }
    inner->addWidget(fanArt);
    inner->addWidget(segments);
    modeText = text("", inner);
    modeNote = text("", inner, "muted");
    manualPanel = new QWidget;
    auto manualLayout = new QVBoxLayout(manualPanel);
    manualLayout->setContentsMargins(0, 4, 0, 0);
    manualLayout->setSpacing(10);
    auto sliderRow = new QHBoxLayout;
    slider->setRange(kFanMin, kFanMax);
    slider->setSingleStep(1);
    slider->setPageStep(5);
    slider->setValue(60);
    slider->setAccessibleName("Velocidade manual das ventoinhas");
    sliderRow->addWidget(slider, 1);
    manualValue = new QLabel("60%");
    manualValue->setObjectName("metric");
    manualValue->setMinimumWidth(78);
    manualValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sliderRow->addWidget(manualValue);
    manualLayout->addLayout(sliderRow);
    auto limits = new QHBoxLayout;
    auto lo = new QLabel("25% · mínimo");
    lo->setObjectName("muted");
    auto hi = new QLabel("100% · máximo");
    hi->setObjectName("muted");
    limits->addWidget(lo);
    limits->addStretch();
    limits->addWidget(hi);
    manualLayout->addLayout(limits);
    row = new QHBoxLayout;
    applyManual = button("Aplicar velocidade", row, [this] { applyManualSpeed(); },
                         true);
    row->addStretch();
    manualLayout->addLayout(row);
    manualHelp = new QLabel;
    manualHelp->setObjectName("callout");
    manualHelp->setWordWrap(true);
    manualHelp->setTextFormat(Qt::PlainText);
    manualLayout->addWidget(manualHelp);
    inner->addWidget(manualPanel);
    connect(slider, &QSlider::valueChanged, this, [this](int value) {
      manualDirty = true;
      manualValue->setText(QString::number(value) + "%");
    });
    tempWarn = new QLabel;
    tempWarn->setObjectName("danger");
    tempWarn->setWordWrap(true);
    tempWarn->setTextFormat(Qt::PlainText);
    tempWarn->hide();
    inner->addWidget(tempWarn);

    panel("Em tempo real", v, inner);
    row = new QHBoxLayout;
    row->addWidget(gaugeCpu, 1);
    row->addWidget(gaugeGpu, 1);
    inner->addLayout(row);
    cpuTempLabel = text("CPU —", inner, "muted");
    cpuTempLabel->setAlignment(Qt::AlignCenter);

    panel("Suporte detectado", v, inner);
    auto support = new QGridLayout;
    support->setHorizontalSpacing(14);
    support->setVerticalSpacing(10);
    pillManual = pill();
    pillProfile = pill();
    pillPower = pill();
    int line = 0;
    for (auto entry : {std::make_pair("Controle manual das ventoinhas", pillManual),
                       std::make_pair("Perfil de plataforma do firmware", pillProfile),
                       std::make_pair("Perfis de energia do sistema", pillPower)}) {
      auto name = new QLabel(entry.first);
      support->addWidget(name, line, 0);
      support->addWidget(entry.second, line, 1, Qt::AlignRight);
      ++line;
    }
    support->setColumnStretch(0, 1);
    inner->addLayout(support);
    backendNote = text("", inner, "muted");
    v->addStretch();

    // ---- Modo gráfico
    v = page("Modo gráfico", "Escolha como o notebook usa as placas de vídeo.",
             Glyph::Gpu);
    panel("Configuração atual", v, inner, &graphicsPill);
    graphicsState = text("Verificando configuração…", inner, "muted");
    panel("Desempenho", v, inner);
    text("Modo híbrido: mantém a placa integrada e permite usar a NVIDIA em "
         "jogos e aplicativos compatíveis.",
         inner);
    row = new QHBoxLayout;
    inner->addLayout(row);
    hybrid = button("Usar Desempenho", row, [this] { perform("gpu-hybrid"); },
                    true);
    row->addStretch();
    panel("Economia", v, inner);
    text("Usa somente a placa integrada. A NVIDIA fica desativada após "
         "reiniciar para reduzir o consumo.",
         inner);
    row = new QHBoxLayout;
    inner->addLayout(row);
    integrated =
        button("Usar Economia", row, [this] { perform("gpu-integrated"); });
    row->addStretch();
    text("As mudanças exigem reinício. No modo Economia, monitores conectados "
         "a saídas ligadas à NVIDIA podem ficar indisponíveis.",
         v, "muted");
    text("Botões desativados? Instale o EnvyControl e o auxiliar do OpenSense "
         "conforme o guia. O projeto original do EnvyControl está arquivado; a "
         "compatibilidade deve ser verificada no seu sistema.",
         v, "muted");
    v->addStretch();

    // ---- Bateria
    v = page("Bateria", "Carga, saúde estimada e proteção em 80%.",
             Glyph::Battery);
    panel("Carga e saúde", v, inner);
    inner->addWidget(batteryArt);
    batteryState = text("Lendo bateria…", inner, "muted");
    inner->addWidget(charge);
    healthLabel = text("Saúde estimada: lendo…", inner, "metric");
    capacityLabel = text("", inner);
    cyclesLabel = text("", inner);
    text("A saúde compara a capacidade atual com a de fábrica. É uma "
         "estimativa do controlador, não um diagnóstico físico; pode variar e "
         "até superar 100%.",
         inner, "muted");
    panel("Proteção da bateria", v, inner, &limitPill);
    inner->addWidget(protectionArt);
    limitState = text("Verificando suporte…", inner);
    row = new QHBoxLayout;
    inner->addLayout(row);
    protect = button("Limitar a 80%", row, [this] { perform("battery-on"); },
                     true);
    unprotect = button("Permitir carga completa", row,
                       [this] { perform("battery-off"); });
    row->addStretch();
    text("Limitar a carga é útil quando o notebook passa muito tempo na tomada. "
         "Se já estiver acima de 80%, ele não descarrega imediatamente. A opção "
         "não bloqueia o botão nem garante persistência após reiniciar.",
         inner, "muted");
    v->addStretch();

    // ---- Ajuda
    v = page("Ajuda", "Compatibilidade, diagnóstico e como habilitar recursos.",
             Glyph::Help);
    panel("Alguma função não apareceu?", v, inner);
    text("O OpenSense só habilita controles disponíveis no sistema. Para "
         "alterar GPU, carga ou ventoinhas, é necessário instalar o auxiliar e "
         "ter Polkit na sessão. O auxiliar de resfriamento dispensa senha na sessão local ativa. Bateria e ventoinhas "
         "também exigem um driver compatível.",
         inner);
    panel("Habilitar as ventoinhas no Nitro V ANV15-51 (experimental)", v,
          inner);
    text("O instalador inclui suporte DKMS específico para Acer Nitro ANV15-51 "
         "com kernel 7.3.x. Outros modelos usam o suporte já disponível no sistema. "
         "Consulte a documentação do driver antes de instalar.", inner);
    auto recipe = new QPlainTextEdit(
        "# Na pasta do projeto, instalar somente o driver do ANV15-51:\n"
        "bash install.sh --driver-only\n"
        "\n"
        "# Depois de reiniciar, verificar o driver e os sensores:\n"
        "bash drivers/status.sh\n"
        "\n"
        "# Voltar ao driver original:\n"
        "sudo bash drivers/remove-driver.sh");
    recipe->setReadOnly(true);
    recipe->setObjectName("code");
    recipe->setMinimumHeight(150);
    recipe->setMaximumHeight(170);
    recipe->setAccessibleName("Comandos para habilitar as ventoinhas");
    inner->addWidget(recipe);
    text("Consulte drivers/README.md antes de instalar. Após reiniciar, "
         "use \"Verificar novamente\" abaixo.",
         inner, "muted");
    panel("Diagnóstico", v, inner);
    details->setReadOnly(true);
    details->setObjectName("code");
    details->setMinimumHeight(240);
    details->setAccessibleName("Relatório de diagnóstico");
    inner->addWidget(details);
    row = new QHBoxLayout;
    inner->addLayout(row);
    button("Exportar diagnóstico", row, [this] {
      auto file = QFileDialog::getSaveFileName(
          this, "Salvar diagnóstico", "opensense-diagnostico.json",
          "JSON (*.json)", nullptr, QFileDialog::DontUseNativeDialog);
      if (file.isEmpty())
        return;
      QSaveFile f(file);
      if (!f.open(QIODevice::WriteOnly) ||
          f.write(QJsonDocument(diagnostic).toJson()) < 0 || !f.commit())
        QMessageBox::warning(this, "Não foi possível salvar", f.errorString());
      else
        notice->setText("Diagnóstico salvo.");
    });
    button("Verificar novamente", row, [this] {
      refresh();
      refreshServices();
    });
    row->addStretch();
    v->addStretch();

    // ---- Sobre
    v = page("Sobre", "Uma necessidade pessoal. Um projeto para compartilhar.", Glyph::About);
    panel("Gabriel Renan", v, inner);
    text("Gabriel Renan Silva Martins Guimarães", inner, "metric");
    text("Criador do OpenSense · estudante de Engenharia da Computação", inner);
    text("Universidade Federal Rural de Pernambuco — Unidade Acadêmica de Belo Jardim (UFRPE/UABJ).", inner, "muted");
    panel("Como nasceu o OpenSense", v, inner);
    text("O OpenSense nasceu de uma dificuldade que encontrei ao usar meu notebook Acer com Linux: "
         "não tive o suporte que esperava para controlar as ventoinhas, gerenciar a bateria e "
         "acessar recursos importantes do equipamento.", inner);
    text("Em vez de me limitar a essa experiência, decidi criar uma ferramenta que reunisse "
         "esses controles em uma interface simples, leve e agradável. O que começou como "
         "uma solução para o meu próprio notebook se tornou um projeto que quero compartilhar "
         "com outras pessoas que enfrentam dificuldades semelhantes no Linux.", inner);
    text("A proposta é dar mais autonomia ao usuário, respeitando o que cada modelo e seus "
         "drivers realmente permitem controlar.", inner);
    panel("Encontre o criador", v, inner);
    row = new QHBoxLayout; inner->addLayout(row);
    auto openProfile = [this](const QString &url) {
      if (!QDesktopServices::openUrl(QUrl(url)))
        notice->setText("Não foi possível abrir o navegador. Copie o endereço exibido nesta página.");
    };
    button("GitHub ↗", row, [openProfile] { openProfile("https://github.com/GabrielRenanPE"); }, true);
    button("LinkedIn ↗", row, [openProfile] { openProfile("https://www.linkedin.com/in/gabrielrenan-computer-eng/"); });
    row->addStretch();
    auto githubLink = text("https://github.com/GabrielRenanPE", inner, "muted");
    auto linkedinLink = text("https://www.linkedin.com/in/gabrielrenan-computer-eng/", inner, "muted");
    githubLink->setTextInteractionFlags(Qt::TextSelectableByMouse);
    linkedinLink->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text("Projeto independente, sem vínculo oficial com a Acer. A disponibilidade dos recursos depende do hardware e dos drivers instalados.", v, "muted");
    v->addStretch();

    connect(nav, &QListWidget::currentRowChanged, stack, [this](int index) {
      stack->setCurrentIndex(index);
      if (auto page = stack->currentWidget()) page->update();
    });
    nav->setCurrentRow(0);

    auto tray = new QSystemTrayIcon(windowIcon(), this);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
      auto menu = new QMenu(this);
      menu->addAction("Abrir OpenSense", this, [this] {
        showNormal();
        raise();
      });
      menu->addAction("Sair", this, &QWidget::close);
      tray->setContextMenu(menu);
      tray->setToolTip("OpenSense");
      tray->show();
      auto hideButton = new MotionButton("Ocultar na bandeja");
      footer->addWidget(hideButton);
      connect(hideButton, &QPushButton::clicked, this, &QWidget::hide);
      connect(tray, &QSystemTrayIcon::activated, this, [this](auto reason) {
        if (reason == QSystemTrayIcon::Trigger) {
          showNormal();
          raise();
        }
      });
    }
    connect(&timer, &QTimer::timeout, this, [this] { refresh(); });
    timer.start(5000);
    connect(&slowTimer, &QTimer::timeout, this, [this] { refreshServices(); });
    slowTimer.start(30000);
    refresh();
    refreshServices();
  }
  void setPage(int index) { nav->setCurrentRow(index); }
  void openManual() { chooseMode("manual"); }
  void closeEvent(QCloseEvent *e) override {
    if (busy()) {
      e->ignore();
      notice->setText("Aguarde a alteração terminar ou cancele a autenticação "
                      "antes de sair.");
      return;
    }
    auto fb = fanBackend();
    auto st = fanState(fb);
    if (!closeAfter && controlReady && fb.available() && st.known &&
        !st.automatic) {
      QMessageBox box(this);
      box.setWindowTitle("Ventoinha em modo manual");
      box.setText("As ventoinhas continuam em velocidade manual depois que o "
                  "OpenSense fecha.");
      box.setInformativeText("Voltar ao modo Automático antes de sair?");
      auto restore =
          box.addButton("Voltar ao Automático e sair", QMessageBox::AcceptRole);
      auto leave = box.addButton("Sair sem alterar", QMessageBox::DestructiveRole);
      box.addButton("Cancelar", QMessageBox::RejectRole);
      box.exec();
      if (box.clickedButton() == restore) {
        closeAfter = true;
        e->ignore();
        perform(QStringList{"fan-auto"});
        return;
      }
      if (box.clickedButton() != leave) {
        e->ignore();
        return;
      }
    }
    for (auto p : {gpuProc, envyProc, profileProc})
      if (p) {
        p->kill();
        p->waitForFinished(1000);
      }
    e->accept();
  }
};

static const char *kStyle = R"(
QWidget {font-family:"Inter","Noto Sans","Segoe UI",sans-serif;font-size:14px;color:#f1f0f8;}
QWidget#root, QWidget#pageSurface {background:#0c111c;}
QFrame#topbar {background:#101522;border-bottom:1px solid #252e43;}
QLabel#eyebrow {color:#b5a0ff;font-size:11px;font-weight:700;letter-spacing:2px;background:transparent;}
QLabel#topMeta {color:#8795ad;font-size:10px;letter-spacing:1px;}
QLabel#navSection {color:#67768f;font-size:10px;font-weight:700;padding:20px 24px 10px;letter-spacing:1px;}
QLabel#deviceName {color:#c5d1e5;font-size:12px;font-weight:600;padding-bottom:6px;}
QLabel {background:transparent;}
QToolTip {background:#18212c;color:#f1f0f8;border:1px solid #36384f;padding:4px 8px;}
QFrame#sidebar {background:#0a0f19;border-right:1px solid #252637;}
QLabel#brand {font-size:20px;font-weight:700;}
QListWidget#nav {background:transparent;border:none;outline:0;}
QListWidget#nav::item {padding:2px 10px;margin:2px 12px;border-radius:9px;color:#9fb0c4;}
QListWidget#nav::item:hover {background:#111924;color:#c5d2e0;}
QListWidget#nav::item:selected {background:#29233f;color:#c4b5fd;border-left:3px solid #a78bfa;}
QListWidget#nav:focus {border:none;}
QLabel#pageTitle {font-size:32px;font-weight:700;color:#f5f4ff;background:transparent;}
QLabel#muted {color:#a1a4b9;}
QLabel#cardTitle {color:#a4b0c5;font-size:12px;font-weight:600;}
QLabel#metric {font-size:26px;font-weight:700;color:#f2f5ff;}
QLabel#panelTitle {font-size:16px;font-weight:650;}
QFrame#card, QFrame#panel {background:#151d2c;border:1px solid #29354b;border-radius:18px;}
QFrame#card:hover {border-color:#506080;}
QFrame#segments {background:#11131b;border:1px solid #2b2d40;border-radius:16px;}
QLabel#notice {background:#0a0f19;border-top:1px solid #252637;color:#9fb0c4;padding:10px 32px;}
QLabel#callout {background:#1a2430;border:1px solid #36384f;border-radius:8px;padding:10px 12px;color:#c5d2e0;}
QLabel#danger {background:#3a1a18;border:1px solid #7a2f29;border-radius:8px;padding:10px 12px;color:#ffb4ab;}
QLabel#pill {border-radius:10px;padding:3px 11px;background:#1a2430;color:#9fb0c4;font-size:12px;}
QLabel#pill[tone="ok"] {background:#29233f;color:#6ee7c3;}
QLabel#pill[tone="warn"] {background:#3a2e12;color:#f0b955;}
QLabel#pill[tone="bad"] {background:#3a1a18;color:#f06a5f;}
QPushButton {background:#222432;border:1px solid #36384f;border-radius:9px;padding:9px 16px;}
QPushButton:hover {background:#2d2e43;}
QPushButton:pressed {background:#343047;}
QMenu {background:#171923;border:1px solid #36384f;padding:6px;}
QMenu::item:selected {background:#29233f;color:#f1f0f8;}
QPushButton:focus {border:2px solid #6ee7c3;}
QPushButton:disabled {color:#5d6b7d;background:#10161e;border-color:#1c2631;}
QPushButton#primary {background:#8262cc;border:1px solid #a78bfa;color:#ffffff;font-weight:600;}
QPushButton#primary:hover {background:#9475dc;}
QPushButton#primary:disabled {color:#6b8a80;background:#12201d;border-color:#1c2f2a;}
QPushButton#segment {background:transparent;border:1px solid transparent;color:#9fb0c4;font-weight:600;}
QPushButton#segment:hover {background:#17212d;color:#f1f0f8;}
QPushButton#segment:checked {background:#29233f;border:1px solid #a78bfa;color:#6ee7c3;}
QPushButton#segment:disabled {color:#4d5a6b;background:transparent;border-color:transparent;}
QPushButton#segment:focus {border:2px solid #6ee7c3;}
QSlider::groove:horizontal {height:6px;background:#243040;border-radius:3px;}
QSlider::sub-page:horizontal {background:#6ee7c3;border-radius:3px;}
QSlider::handle:horizontal {background:#f1f0f8;width:18px;height:18px;margin:-7px 0;border-radius:10px;border:2px solid #6ee7c3;}
QSlider::handle:horizontal:focus {border:3px solid #8fe6c6;}
QSlider::sub-page:horizontal:disabled {background:#34414f;}
QSlider::handle:horizontal:disabled {background:#3a4656;border-color:#3a4656;}
QProgressBar {background:#11131b;border:1px solid #2b2d40;border-radius:7px;text-align:center;min-height:20px;color:#f1f0f8;}
QProgressBar::chunk {background:#8262cc;border-radius:6px;}
QPlainTextEdit {background:#11131b;border:1px solid #2b2d40;border-radius:9px;padding:10px;}
QPlainTextEdit#code {font-family:"JetBrains Mono","Noto Sans Mono","DejaVu Sans Mono",monospace;font-size:12px;color:#c5d2e0;}
QCheckBox::indicator {width:18px;height:18px;border:1px solid #3a4a5e;border-radius:5px;background:#11131b;}
QCheckBox::indicator:checked {background:#8262cc;border-color:#6ee7c3;}
QCheckBox:focus {color:#6ee7c3;}
QScrollArea {background:#0c111c;border:none;}
QScrollBar:vertical {background:transparent;width:10px;margin:2px;}
QScrollBar::handle:vertical {background:#2a3646;border-radius:4px;min-height:36px;}
QScrollBar::handle:vertical:hover {background:#3a4a5e;}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {height:0;}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {background:transparent;}
QMessageBox {background:#171923;}
)";

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("OpenSense");
  app.setOrganizationName("OpenSense");
  app.setStyle("Fusion");
  QPalette palette;
  palette.setColor(QPalette::Window, QColor("#0c111c"));
  palette.setColor(QPalette::WindowText, QColor("#f1f0f8"));
  palette.setColor(QPalette::Base, QColor("#11131b"));
  palette.setColor(QPalette::AlternateBase, QColor("#171923"));
  palette.setColor(QPalette::Text, QColor("#f1f0f8"));
  palette.setColor(QPalette::Button, QColor("#222432"));
  palette.setColor(QPalette::ButtonText, QColor("#f1f0f8"));
  palette.setColor(QPalette::Highlight, QColor("#7050b0"));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  palette.setColor(QPalette::PlaceholderText, QColor("#a1a4b9"));
  palette.setColor(QPalette::ToolTipBase, QColor("#18212c"));
  palette.setColor(QPalette::ToolTipText, QColor("#f1f0f8"));
  app.setPalette(palette);
  app.setStyleSheet(kStyle);
  Window w;
  w.show();
  QStringList args = app.arguments();
  int tab = args.indexOf("--tab");
  if (tab >= 0 && tab + 1 < args.size())
    w.setPage(args[tab + 1].toInt());
  if (args.contains("--open-manual"))
    w.openManual();
  if (args.contains("--smoke-test"))
    QTimer::singleShot(1800, &w, &QWidget::close);
  int screenshot = args.indexOf("--screenshot");
  if (screenshot >= 0 && screenshot + 1 < args.size())
    QTimer::singleShot(900, &w, [&w, args, screenshot] {
      w.grab().save(args[screenshot + 1]);
    });
  return app.exec();
}
