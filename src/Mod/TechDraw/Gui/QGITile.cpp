/***************************************************************************
 *   Copyright (c) 2019 WandererFan <wandererfan@gmail.com>                *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
#include <QPainter>
#include <QGraphicsItem>
#include <QStyleOptionGraphicsItem>
#include <QFile>
#include <QFileInfo>
#endif

#include <App/Application.h>
#include <App/Material.h>
#include <Base/Console.h>
#include <Base/Parameter.h>

#include <Mod/TechDraw/App/DrawUtil.h>
#include <Mod/TechDraw/App/DrawTile.h>
#include <Mod/TechDraw/App/DrawTileWeld.h>
#include <Mod/TechDraw/App/DrawWeldSymbol.h>

#include <qmath.h>
#include "Rez.h"
#include "DrawGuiUtil.h"
#include "QGIView.h"
#include "QGIWeldSymbol.h"
#include "QGITile.h"

using namespace TechDrawGui;

QGITile::QGITile() :
    m_textL(QString::fromUtf8(" ")),
    m_textR(QString::fromUtf8(" ")),
    m_textC(QString::fromUtf8(" ")),
    m_scale(1.0),
    m_row(0),
    m_col(0),
    m_tailRight(true),
    m_altWeld(false)
{
    m_qgSvg = new QGCustomSvg();
    addToGroup(m_qgSvg);

//    m_effect = new QGraphicsColorizeEffect();

    m_qgTextL = new QGCustomText();
    addToGroup(m_qgTextL);
    
    m_qgTextR = new QGCustomText();
    addToGroup(m_qgTextR);

    m_qgTextC = new QGCustomText();
    addToGroup(m_qgTextC);

    m_wide = getSymbolWidth();
    m_high = prefFontSize();
    m_textL  = QString();
    m_textR  = QString();
    m_textC  = QString();
    m_fontName = prefTextFont();
    m_font = QFont(m_fontName);

#if PY_MAJOR_VERSION < 3
    setHandlesChildEvents(true);    //qt4 deprecated in qt5
#else
    setFiltersChildEvents(true);    //qt5
#endif
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, false);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges,true);
    setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    
    m_colNormal = prefNormalColor();
    m_colCurrent = m_colNormal;
}

QGITile::~QGITile(void)
{

}

QVariant QGITile::itemChange(GraphicsItemChange change, const QVariant &value)
{
//    Base::Console().Message("QGIT::itemChange(%d)\n", change);
    return QGIDecoration::itemChange(change, value);
}

void QGITile::draw(void)
{
//    Base::Console().Message("QGIT::draw()\n");

    prepareGeometryChange();
    m_wide = getSymbolWidth();
    m_high = getSymbolHeight() * scaleToFont();

    makeText();
    makeSymbol();

    double textWidthL = m_qgTextL->boundingRect().width();
    double textWidthR = m_qgTextR->boundingRect().width();
    double totalWidth = m_wide + textWidthL + textWidthR;
    if (m_row == 0) {     //arrowSide
        double x = m_origin.x();
        double y = m_origin.y() - (m_high * 0.5);    //inverted y!!
        setPos(x,y);
    } else if (m_row == -1) {    //otherSide
        if (getAltWeld()) {
            if (isTailRight()) { 
                double x = m_origin.x() + (0.5 * totalWidth); //move to right 1/2 tile width
                double y = m_origin.y() +  (m_high * 0.5);    //inverted y!!
                setPos(x,y);
            } else {
                double x = m_origin.x() - (0.5 * totalWidth); //move to left 1/2 tile width
                double y = m_origin.y() +  (m_high * 0.5);    //inverted y!!
                setPos(x,y);
            }
        } else {
            double x = m_origin.x();
            double y = m_origin.y() + (m_high * 0.5);    //inverted y!!
            setPos(x,y);
        }
    } else {
        double x = m_origin.x() + m_col * totalWidth;
        double y = m_origin.y() - (m_row * m_high) - (m_high * 0.5);    //inverted y!!
        setPos(x,y);
    }
}

void QGITile::makeSymbol(void)
{
//    Base::Console().Message("QGIT::makeSymbol()\n");
//    m_effect->setColor(m_colCurrent);

    if (m_svgPath.isEmpty()) {
        Base::Console().Warning("QGIT::makeSymbol - no symbol file set\n");
        return;
    }

//    m_qgSvg->setGraphicsEffect(m_effect);
    
    QFileInfo fi(m_svgPath);
    if (fi.isReadable()) {
        QFile svgFile(m_svgPath);
        if(svgFile.open(QIODevice::ReadOnly)) {
            QByteArray qba = svgFile.readAll();
            if (!m_qgSvg->load(&qba)) {
                Base::Console().Error("Error - Could not load SVG renderer with **%s**\n", qPrintable(m_svgPath));
                return;
            }
            svgFile.close();
            m_qgSvg->setScale(scaleToFont());
            m_qgSvg->centerAt(0.0, 0.0);   //(0,0) is based on symbol size
        } else {
            Base::Console().Error("Error - Could not open file **%s**\n", qPrintable(m_svgPath));  
        } 
    } else {
        Base::Console().Error("QGIT::makeSymbol - file: **%s** is not readable\n",qPrintable(m_svgPath));
        return;
    }
}

void QGITile::makeText(void)
{
//    Base::Console().Message("QGIT::makeText()\n");
    prepareGeometryChange();
    m_font.setPixelSize(prefFontSize());
    double verticalFudge = 0.10;

    //(0, 0) is 1/2 up (above line symbol)!
    m_qgTextL->setFont(m_font);
    m_qgTextL->setPlainText(m_textL);
    m_qgTextL->setColor(m_colCurrent);
    double textWidth = m_qgTextL->boundingRect().width();
    double charWidth = textWidth / m_textL.size();   //not good for non-ASCII chars
    double hMargin = 1;
    if (!m_textL.isEmpty()) {
        hMargin = (m_wide / 2.0) + (charWidth / 2.0);
    }

    double textHeightL = m_qgTextL->boundingRect().height();
    double vOffset = 0.0;
    if (m_row < 0) {                      // below line
        vOffset = textHeightL * verticalFudge;
    } else {
        vOffset = 0.0;
    }
    m_qgTextL->justifyRightAt(-hMargin, vOffset, true);

    m_qgTextR->setFont(m_font);
    m_qgTextR->setPlainText(m_textR);
    m_qgTextR->setColor(m_colCurrent);
    textWidth = m_qgTextR->boundingRect().width();
    charWidth = textWidth / m_textR.size();
    hMargin = 1;
    if (!m_textR.isEmpty()) {
        hMargin = (m_wide / 2.0) + (charWidth / 2.0);
    }
    double textHeightR = m_qgTextR->boundingRect().height();
    if (m_row < 0) {                      // below line
        vOffset =  textHeightR * verticalFudge;
    } else {
        vOffset = 0.0;
    }
    m_qgTextR->justifyLeftAt(hMargin, vOffset, true);

    m_qgTextC->setFont(m_font);
    m_qgTextC->setPlainText(m_textC);
    m_qgTextC->setColor(m_colCurrent);
    double textHeightC = m_qgTextC->boundingRect().height();
    textHeightC = textHeightC;
    if (m_row < 0) {                      // below line
        vOffset = m_high  * (1 + verticalFudge);
    } else {
        vOffset = -0.5 * (m_high + textHeightC);
    }
    m_qgTextC->centerAt(0.0, vOffset);
}

void QGITile::setTilePosition(QPointF org, int r, int c)
{
    m_origin = org;
    m_row = r;
    m_col = c;
}

void QGITile::setTileScale(double s)
{
    m_scale = s;
}

void QGITile::setTileTextLeft(std::string s)
{
    m_textL = QString::fromUtf8(s.c_str());
}

void QGITile::setTileTextRight(std::string s)
{
    m_textR = QString::fromUtf8(s.c_str());
}

void QGITile::setTileTextCenter(std::string s)
{
    m_textC = QString::fromUtf8(s.c_str());
}

//using label font and dimension font size.  could change later
//void QGITile::setFont(QFont f, double fsize)
//{
//    m_font = f;
//    m_textSize = fsize;
//}

void QGITile::setSymbolFile(std::string s)
{
//    Base::Console().Message("QGIT::setSymbolFile(%s)\n",s.c_str());
    if (!s.empty()) {
        m_svgPath = QString::fromUtf8(s.c_str());
    }
}

void QGITile::setPrettyNormal() {
    m_colCurrent = m_colNormal;

//    m_effect->setColor(m_colNormal);
    m_qgTextL->setColor(m_colNormal);
    m_qgTextR->setColor(m_colNormal);
    m_qgTextC->setColor(m_colNormal);

    draw();
}

void QGITile::setPrettyPre() {
    m_colCurrent = prefPreColor();

//    m_effect->setColor(m_colCurrent);
    m_qgTextL->setColor(m_colCurrent);
    m_qgTextR->setColor(m_colCurrent);
    m_qgTextC->setColor(m_colCurrent);

    draw();
}

void QGITile::setPrettySel() {
    m_colCurrent = prefSelectColor();

//    m_effect->setColor(m_colCurrent);
    m_qgTextL->setColor(m_colCurrent);
    m_qgTextR->setColor(m_colCurrent);
    m_qgTextC->setColor(m_colCurrent);

    draw();
}

bool QGITile::isTailRight(void) 
{
    return m_tailRight;
}

bool QGITile::getAltWeld(void) 
{
    return m_altWeld;
}

//TODO: this is Pen, not Brush. sb Brush to colour background
QColor QGITile::getTileColor(void) const
{
    Base::Reference<ParameterGrp> hGrp = App::GetApplication().GetUserParameter()
        .GetGroup("BaseApp")->GetGroup("Preferences")->GetGroup("Mod/TechDraw/Colors");
    App::Color fcColor = App::Color((uint32_t) hGrp->GetUnsigned("TileColor", 0x00000000));
    return fcColor.asValue<QColor>();
}

double QGITile::getSymbolWidth(void) const
{
    Base::Reference<ParameterGrp> hGrp = App::GetApplication().GetUserParameter().GetGroup("BaseApp")->
                                         GetGroup("Preferences")->GetGroup("Mod/TechDraw/Dimensions");
    double w = hGrp->GetFloat("SymbolSize",64);
//     symbols are only nominally 64x64. they actually have a "border" of 4 - 0.5*stroke(0.5)
//     so we'll say effectively 62x62? 60 x 60
//    double w = 64.0;
    double fudge = 4.0;              //allowance for tile border
    w = w - fudge;
    return w;
}

double QGITile::getSymbolHeight(void) const
{
    Base::Reference<ParameterGrp> hGrp = App::GetApplication().GetUserParameter().GetGroup("BaseApp")->
                                         GetGroup("Preferences")->GetGroup("Mod/TechDraw/Dimensions");
    double h = hGrp->GetFloat("SymbolSize",64);
    double fudge = 4.0;
    h = h - fudge;
//    double h = 60.0;
    return h;
}

double QGITile::getSymbolFactor(void) const
{
    Base::Reference<ParameterGrp> hGrp = App::GetApplication().GetUserParameter().GetGroup("BaseApp")->
                                         GetGroup("Preferences")->GetGroup("Mod/TechDraw/Decorations");
    double s = hGrp->GetFloat("SymbolFactor",1.25);
//    double s = 1.25;
    return s;
}

double QGITile::prefFontSize(void) const
{
    Base::Reference<ParameterGrp> hGrp = App::GetApplication().GetUserParameter().
                       GetGroup("BaseApp")->GetGroup("Preferences")->GetGroup("Mod/TechDraw/Dimensions");
    double sizeMM = hGrp->GetFloat("FontSize", QGIView::DefaultFontSizeInMM);
    double fontSize = QGIView::calculateFontPixelSize(sizeMM);
    return fontSize;
}

//factor to scale symbol to match font size
double QGITile::scaleToFont(void) const
{
    double fpx = prefFontSize();
    double spx = getSymbolHeight();
    double factor = getSymbolFactor();
    double sf = (fpx / spx) * factor;
    return sf;
}

QString QGITile::prefTextFont(void) const
{
    Base::Reference<ParameterGrp> hGrp = App::GetApplication().GetUserParameter().
                                         GetGroup("BaseApp")->GetGroup("Preferences")->GetGroup("Mod/TechDraw/Labels");
    std::string fontName = hGrp->GetASCII("LabelFont", "osifont");
    return QString::fromStdString(fontName);
}

void QGITile::paint ( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget) {
    QStyleOptionGraphicsItem myOption(*option);
    myOption.state &= ~QStyle::State_Selected;

//    painter->setPen(Qt::magenta);
//    painter->drawRect(boundingRect());          //good for debugging
    
    QGIDecoration::paint (painter, &myOption, widget);
}

QRectF QGITile::boundingRect() const
{
    return childrenBoundingRect();
}

