/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef QWIDGET_H_
#define QWIDGET_H_

#include <qobject.h>
#include <qpaintdevice.h>
#include <qpainter.h>
#include <qpoint.h>
#include <qsize.h>
#include <qpalette.h>
#include <qfont.h>
#include <qcursor.h>
#include <qevent.h>
#include <KWQStyle.h>

#ifdef __OBJC__
@class NSView;
#else
class NSView;
#endif

class QWidgetPrivate;

class QWidget : public QObject, public QPaintDevice {
public:

    enum WidgetFlags {
	WResizeNoErase = 1,
	WRepaintNoErase = 1 << 1,
	WPaintUnclipped = 1 << 2
    };

    enum FocusPolicy {
        NoFocus = 0,
        TabFocus = 0x1,
        ClickFocus = 0x2,
        StrongFocus = 0x3,
        WheelFocus = 0x7
    };

    QWidget(QWidget *parent = 0, const char *name = 0, int f = 0);
    ~QWidget();

    virtual QSize sizeHint() const;
    virtual void resize(int,int);
    virtual void setActiveWindow();
    virtual void setEnabled(bool);
    virtual void setAutoMask(bool);
    virtual void setMouseTracking(bool);

    int winId() const;
    int x() const;
    int y() const;
    int width() const;
    int height() const;
    QSize size() const;
    void resize(const QSize &);
    QPoint pos() const;
    void move(int, int);
    virtual void move(const QPoint &);

    QRect frameGeometry() const;

    QWidget *topLevelWidget() const;

    QPoint mapToGlobal(const QPoint &) const;
    QPoint mapFromGlobal(const QPoint &) const;

    void setFocus();
    void clearFocus();
    FocusPolicy focusPolicy() const;
    virtual void setFocusPolicy(FocusPolicy);
    virtual void setFocusProxy( QWidget * );

    const QPalette& palette() const;
    virtual void setPalette(const QPalette &);
    void unsetPalette();
    
    QStyle &style() const;
    void setStyle(QStyle *);
    
    QFont font() const;
    virtual void setFont(const QFont &);
    
    void constPolish() const;
    virtual QSize minimumSizeHint() const;
    bool isVisible() const;
    virtual void setCursor(const QCursor &);
    QCursor cursor();
    void unsetCursor();
    bool event(QEvent *);
    bool focusNextPrevChild(bool);
    bool hasMouseTracking() const;

    virtual void show();
    virtual void hide();

    virtual void showEvent(QShowEvent *);
    virtual void hideEvent(QHideEvent *);
    virtual void wheelEvent(QWheelEvent *);
    virtual void keyPressEvent(QKeyEvent *);
    virtual void keyReleaseEvent(QKeyEvent *);
    virtual void focusOutEvent(QFocusEvent *);

    enum BackgroundMode { NoBackground };    
    void setBackgroundMode(BackgroundMode) { }

    void setAcceptDrops(bool) { }
    
    virtual void paint(void *);

    void _displayRect(QRect rect);
    
    NSView *getView() const;
    void setView(NSView *aView);
    
    void endEditing();

private:
    void internalSetGeometry( int x, int y, int w, int h );

    QWidgetPrivate *data;
};

#endif
