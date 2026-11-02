#pragma once
#include "motion.hpp"
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <cmath>

// Native vector art: no image decoding, web view or whole-page effects.
inline QColor modeColor(const QString &mode) {
  return mode == "eco" ? QColor("#6ee7c3") : mode == "performance" ? QColor("#ffb76b")
       : mode == "manual" ? QColor("#bdaaff") : QColor("#78ceff");
}
inline void drawModeSymbol(QPainter &p, const QString &mode, const QRectF &r, double phase=0) {
  p.save(); p.translate(r.center()); p.scale(r.width()/48, r.height()/48);
  p.setPen(QPen(modeColor(mode), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  if (mode == "eco") {
    p.rotate(5*std::sin(phase));
    QPainterPath leaf; leaf.moveTo(-13,13); leaf.cubicTo(-22,-6,0,-19,17,-17);
    leaf.cubicTo(17,3,4,23,-13,13); p.drawPath(leaf);
    p.drawLine(QPointF(-16,17),QPointF(9,-9));
    p.drawLine(QPointF(-5,5),QPointF(-7,-4));
  } else if (mode == "auto") {
    p.translate(0, 1.5*std::sin(phase));
    p.drawRoundedRect(QRectF(-16,-11,32,27),7,7);
    p.drawLine(QPointF(0,-11),QPointF(0,-18)); p.drawEllipse(QPointF(0,-20),2,2);
    p.drawLine(QPointF(-20,-3),QPointF(-20,7));p.drawLine(QPointF(20,-3),QPointF(20,7));
    p.setBrush(modeColor(mode));p.drawEllipse(QPointF(-6,0),2,3);p.drawEllipse(QPointF(6,0),2,3);
    p.drawLine(QPointF(-5,9),QPointF(5,9));
  } else if (mode == "performance") {
    p.translate(0,-2*std::sin(phase));p.rotate(25);
    QPainterPath rocket;rocket.moveTo(0,-21);rocket.cubicTo(15,-10,12,4,7,12);
    rocket.lineTo(-7,12);rocket.cubicTo(-12,4,-15,-10,0,-21);p.drawPath(rocket);
    p.drawEllipse(QPointF(0,-5),4,4);
    p.drawLine(QPointF(-9,3),QPointF(-17,13));p.drawLine(QPointF(9,3),QPointF(17,13));
    p.setPen(QPen(QColor("#ffb76b"),3,Qt::SolidLine,Qt::RoundCap));
    p.drawLine(QPointF(0,16),QPointF(0,21+3*std::sin(phase)));
  } else if (mode == "manual") {
    p.drawEllipse(QPointF(0,-11),7,7);
    QPainterPath body;body.moveTo(-16,17);body.cubicTo(-16,-2,16,-2,16,17);p.drawPath(body);
    p.drawLine(QPointF(-10,17),QPointF(10,17));
  } else {
    p.drawEllipse(QPointF(0,0),16,16);
    p.drawLine(QPointF(-5,0),QPointF(5,0));
  }
  p.restore();
}
inline QIcon modeSymbolIcon(const QString &mode) {
  QPixmap px(64,64);px.setDevicePixelRatio(2);px.fill(Qt::transparent);
  QPainter p(&px);p.setRenderHint(QPainter::Antialiasing);drawModeSymbol(p,mode,QRectF(2,2,28,28));
  p.end();return QIcon(px);
}

class StatusArt : public QWidget {
public:
  enum class Kind { Fan, Battery, Protection };
private:
  Kind kind;
  QString state, heading, detail;
  double angle=0, phase=0;
  int percent=-1;
  QTimer tick;
  bool animatable() const {
    return kind == Kind::Fan ? !state.isEmpty()
      : kind == Kind::Protection ? (state=="on" || state=="off")
      : (state=="Charging" || state=="Discharging");
  }
public:
  explicit StatusArt(Kind k) : kind(k) {
    setMinimumHeight(138);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    tick.setInterval(33);
    connect(&tick,&QTimer::timeout,this,[this] {
      phase+=0.08;
      angle=std::fmod(angle+(state=="performance" ? 10 : state=="eco" ? 2 : 5),360.0);
      update();
    });
  }
  void setState(const QString &s,const QString &title,const QString &subtitle,int level=-1) {
    if(state!=s) {phase=0;angle=0;}
    state=s;heading=title;detail=subtitle;percent=level;
    setAccessibleName(title);setAccessibleDescription(subtitle);
    syncMotion();
  }
  void syncMotion() {
    if(isVisible() && motionEnabled() && animatable()) tick.start();
    else {tick.stop();phase=0;angle=0;}
    update();
  }
  bool animationRunning() const {return tick.isActive();}
protected:
  void showEvent(QShowEvent *e) override {QWidget::showEvent(e);syncMotion();}
  void hideEvent(QHideEvent *e) override {tick.stop();QWidget::hideEvent(e);}
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    const double t=motionEnabled()?phase:0;
    const QColor accent=kind==Kind::Fan?modeColor(state):state=="off"?QColor("#78ceff"):QColor("#6ee7c3");
    p.setPen(Qt::NoPen);p.setBrush(QColor("#111925"));p.drawRoundedRect(rect().adjusted(0,0,-1,-1),16,16);
    p.save();p.translate(65,69);
    p.setPen(QPen(QColor("#2c3b50"),2));p.setBrush(Qt::NoBrush);p.drawEllipse(QPointF(0,0),46,46);
    if(kind==Kind::Fan) {
      p.save();p.rotate(motionEnabled()?angle:0);p.setPen(Qt::NoPen);
      p.setBrush(state.isEmpty()?QColor("#566475"):accent);
      for(int i=0;i<5;++i) {
        p.rotate(72);QPainterPath blade;blade.moveTo(0,-8);blade.cubicTo(-25,-38,17,-43,14,-19);
        blade.cubicTo(12,-9,5,-6,0,-8);p.drawPath(blade);
      }
      p.setBrush(QColor("#111925"));p.drawEllipse(QPointF(0,0),9,9);
      p.setBrush(accent);p.drawEllipse(QPointF(0,0),4,4);p.restore();
      p.setPen(QPen(QColor("#2c3b50"),1));p.setBrush(QColor("#111925"));p.drawEllipse(QPointF(36,31),23,23);
      drawModeSymbol(p,state,QRectF(18,13,36,36),t);
    } else {
      p.setPen(QPen(accent,2.5));p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(QRectF(-31,-17,58,34),5,5);p.drawLine(QPointF(32,-6),QPointF(32,6));
      if(percent>=0) {
        p.setPen(Qt::NoPen);p.setBrush(accent);
        p.drawRoundedRect(QRectF(-26,-12,48*std::clamp(percent/100.0,0.0,1.0),24),2,2);
      }
      p.setPen(QPen(QColor("#111925"),2));
      if(kind==Kind::Protection) {
        p.setPen(QPen(accent,2));p.setBrush(QColor("#111925"));
        QPainterPath shield;shield.moveTo(20,8);shield.lineTo(38,14);shield.lineTo(36,32);
        shield.quadTo(30,42,20,46);shield.quadTo(10,42,4,32);shield.lineTo(2,14);shield.closeSubpath();
        p.drawPath(shield);
        if(state=="on") {p.drawLine(QPointF(12,26),QPointF(18,32));p.drawLine(QPointF(18,32),QPointF(29,20));}
        else if(state=="off") {p.drawLine(QPointF(13,27),QPointF(27,27));p.drawLine(QPointF(22,22),QPointF(27,27));p.drawLine(QPointF(22,32),QPointF(27,27));}
        p.setPen(QPen(accent,1));p.setOpacity(0.3+0.25*(1+std::sin(t)));
        p.drawEllipse(QPointF(20,27),25,25);
      } else if(state=="Charging") {
        p.setPen(QPen(QColor("#111925"),3,Qt::SolidLine,Qt::RoundCap));
        QPainterPath bolt;bolt.moveTo(1,-10);bolt.lineTo(-5,1);bolt.lineTo(4,1);bolt.lineTo(-1,10);p.drawPath(bolt);
        p.setOpacity(0.3+0.3*(1+std::sin(t)));p.setPen(QPen(accent,2));p.drawArc(QRectF(-43,-43,86,86),int(t*40)*16,65*16);
      } else if(state=="Discharging") {
        p.setPen(QPen(accent,2));const double x=5*std::sin(t);
        p.drawLine(QPointF(-8+x,28),QPointF(12+x,28));p.drawLine(QPointF(6+x,23),QPointF(12+x,28));p.drawLine(QPointF(6+x,33),QPointF(12+x,28));
      }
    }
    p.restore();p.setOpacity(1);p.setPen(QColor("#f1f0f8"));
    QFont f=font();f.setPixelSize(18);f.setBold(true);p.setFont(f);
    p.drawText(QRectF(137,25,width()-153,35),Qt::AlignVCenter,heading);
    f.setPixelSize(12);f.setBold(false);p.setFont(f);p.setPen(QColor("#a8b6ca"));
    p.drawText(QRectF(137,65,width()-153,60),Qt::AlignTop|Qt::TextWordWrap,detail);
  }
};
