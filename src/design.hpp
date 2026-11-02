#pragma once
#include "motion.hpp"
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

// Paint only this surface: no offscreen capture of stacked/scrolling pages.
class SectionHeader : public QFrame {
  QVariantAnimation reveal;
  double progress = 1;
public:
  SectionHeader(const QString &title, const QString &subtitle, int index) {
    setMinimumHeight(146);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 20, 112, 22);
    layout->setSpacing(8);
    auto eyebrow = new QLabel(QString("OPENSENSE  /  %1").arg(index + 1, 2, 10, QChar('0')));
    eyebrow->setObjectName("eyebrow");
    layout->addWidget(eyebrow);
    auto heading = new QLabel(title);
    heading->setObjectName("pageTitle");
    heading->setWordWrap(true);
    layout->addWidget(heading);
    auto sub = new QLabel(subtitle);
    sub->setObjectName("muted"); sub->setWordWrap(true);
    layout->addWidget(sub);
    reveal.setDuration(260); reveal.setEasingCurve(QEasingCurve::OutCubic);
    connect(&reveal, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
      progress = v.toDouble(); update();
    });
  }
  void showEvent(QShowEvent *e) override {
    reveal.stop(); progress = 1;
    if (motionEnabled()) { reveal.setStartValue(0.0); reveal.setEndValue(1.0); reveal.start(); }
    QFrame::showEvent(e);
  }
  void hideEvent(QHideEvent *e) override { reveal.stop(); QFrame::hideEvent(e); }
  void paintEvent(QPaintEvent *) override {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QRectF area = rect().adjusted(1, 1, -1, -1);
    QLinearGradient g(0, 0, width(), height());
    g.setColorAt(0, QColor("#24253d")); g.setColorAt(0.65, QColor("#181e2e"));
    g.setColorAt(1, QColor("#13272b"));
    p.setPen(QPen(QColor("#363d55"), 1)); p.setBrush(g);
    p.drawRoundedRect(area, 20, 20);
    p.save(); p.setOpacity(0.18);
    drawShuriken(p, QRectF(width() - 108, 24, 84, 84), 15);
    p.restore();
    p.setPen(QPen(QColor("#b5a0ff"), 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(27, height() - 11),
               QPointF(27 + 84 * (motionEnabled() ? progress : 1), height() - 11));
  }
};

class MetricFrame : public QFrame {
public:
  MetricFrame() { setObjectName("card"); }
  void paintEvent(QPaintEvent *e) override {
    QFrame::paintEvent(e);
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#526389"), 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(22, 1), QPointF(54, 1));
  }
};
