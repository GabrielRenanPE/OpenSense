#pragma once
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QEnterEvent>
#include <QSettings>
#include <QPushButton>

inline bool motionEnabled() {
  return !QSettings().value("appearance/reduceMotion", false).toBool();
}

inline void drawShuriken(QPainter &p, const QRectF &bounds, double angle = 0) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(bounds.center());
  const double scale = std::min(bounds.width(), bounds.height()) / 64.0;
  p.scale(scale, scale);
  p.rotate(angle);
  QPainterPath shape;
  shape.moveTo(0, -29); shape.lineTo(7, -9); shape.lineTo(29, 0);
  shape.lineTo(9, 7); shape.lineTo(0, 29); shape.lineTo(-7, 9);
  shape.lineTo(-29, 0); shape.lineTo(-9, -7); shape.closeSubpath();
  shape.addEllipse(QPointF(0, 0), 5, 5);
  shape.setFillRule(Qt::OddEvenFill);
  QLinearGradient gradient(-23, -23, 23, 23);
  gradient.setColorAt(0, QColor("#bdaaff"));
  gradient.setColorAt(0.52, QColor("#a78bfa"));
  gradient.setColorAt(1, QColor("#6ee7c3"));
  p.fillPath(shape, gradient);
  p.restore();
}

inline QIcon shurikenIcon() {
  QIcon icon;
  for (int size : {16, 24, 32, 48, 64, 128, 256}) {
    QPixmap px(size, size); px.fill(Qt::transparent);
    QPainter p(&px); drawShuriken(p, QRectF(0, 0, size, size)); p.end();
    icon.addPixmap(px);
  }
  return icon;
}

class ShurikenMark : public QWidget {
  QVariantAnimation turn;
  double angle = 0;
public:
  ShurikenMark() {
    setFixedSize(42, 42);
    setAccessibleName("OpenSense — shuriken");
    turn.setDuration(420);
    turn.setEasingCurve(QEasingCurve::OutCubic);
    connect(&turn, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
      angle = v.toDouble(); update();
    });
  }
  void enterEvent(QEnterEvent *e) override {
    if (motionEnabled()) {
      turn.stop(); turn.setStartValue(angle); turn.setEndValue(angle + 90); turn.start();
    }
    QWidget::enterEvent(e);
  }
  void hideEvent(QHideEvent *e) override { turn.stop(); QWidget::hideEvent(e); }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this); drawShuriken(p, rect(), angle);
  }
};

// A short overlay responds to pointer and keyboard focus without layout changes.
class MotionButton : public QPushButton {
  QVariantAnimation glow;
  double strength = 0;
  void animate(bool active) {
    glow.stop();
    if (!motionEnabled()) { strength = 0; update(); return; }
    glow.setStartValue(strength); glow.setEndValue(active ? 1.0 : 0.0); glow.start();
  }
public:
  explicit MotionButton(const QString &label) : QPushButton(label) {
    glow.setDuration(150); glow.setEasingCurve(QEasingCurve::OutCubic);
    connect(&glow, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
      strength = v.toDouble(); update();
    });
  }
  void enterEvent(QEnterEvent *e) override { animate(true); QPushButton::enterEvent(e); }
  void leaveEvent(QEvent *e) override { animate(hasFocus()); QPushButton::leaveEvent(e); }
  void focusInEvent(QFocusEvent *e) override { animate(true); QPushButton::focusInEvent(e); }
  void focusOutEvent(QFocusEvent *e) override { animate(underMouse()); QPushButton::focusOutEvent(e); }
  void hideEvent(QHideEvent *e) override { glow.stop(); strength = 0; QPushButton::hideEvent(e); }
  void paintEvent(QPaintEvent *e) override {
    QPushButton::paintEvent(e);
    if (!isEnabled() || strength <= 0 || !motionEnabled()) return;
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(QColor(167, 139, 250, int(18 * strength)));
    p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 9, 9);
  }
};
