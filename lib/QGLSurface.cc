/* -*- c++ -*- */
/*
 * Copyright 2013-2021 Sylvain Munaut <tnt@246tNt.com>
 *
 * This file is part of gr-fosphor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QtEvents>
#include <QThread>

#include "QGLSurface.h"
#include "qt_sink_c_impl.h"

#include <stdio.h>

namespace gr {
  namespace fosphor {

QGLSurface::QGLSurface(QWidget *parent, qt_sink_c_impl *block)
  : QGLWidget(parent), d_block(block)
{
	/* Save the pointer to the main GUI thread */
	this->d_gui_thread = this->thread();

	/*
	 * This widget is a native window (it has its own GL surface). By default
	 * Qt also makes all its ancestors native. On Wayland those ancestors
	 * become sub-surfaces that never get a buffer attached (the content of
	 * non-toplevel widgets is painted in the toplevel's buffer), and a
	 * sub-surface whose parent is unmapped is not displayed. The result is
	 * a blank box when the widget isn't a direct child of the toplevel.
	 */
	this->setAttribute(Qt::WA_NativeWindow);
	this->setAttribute(Qt::WA_DontCreateNativeAncestors);

	/* QWidget policies */
	this->setFocusPolicy(Qt::StrongFocus);
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}


void
QGLSurface::hideEvent(QHideEvent *he)
{
	this->d_block->cb_visibility(this->isVisible());
}

void
QGLSurface::showEvent(QShowEvent *he)
{
	this->d_block->cb_visibility(this->isVisible());
}

void
QGLSurface::paintEvent(QPaintEvent *pe)
{
	/* Don't do anything */

	/*
	 * The default implementation calls makeCurrent but here we want
	 * _other_ threads to be current, so we need a dummy empty impl
	 * for the paintEvent
	 */
}

void
QGLSurface::resizeEvent(QResizeEvent *re)
{
	/*
	 * The default implementation calls makeCurrent but here we want
	 * _other_ threads to be current, so don't do that !
	 */

	/*
	 * Call back to main block
	 * (the GL surface is in device pixels, which differs from the widget
	 *  size in logical pixels on HiDPI / scaled displays)
	 */
	qreal dpr = this->devicePixelRatioF();
	this->d_block->cb_reshape(
		qRound(re->size().width()  * dpr),
		qRound(re->size().height() * dpr)
	);
}

void
QGLSurface::keyPressEvent(QKeyEvent *ke)
{
	switch (ke->key()) {
	case Qt::Key_Up:
		this->d_block->execute_ui_action(qt_sink_c_impl::REF_DOWN);
		break;
	case Qt::Key_Down:
		this->d_block->execute_ui_action(qt_sink_c_impl::REF_UP);
		break;
	case Qt::Key_Left:
		this->d_block->execute_ui_action(qt_sink_c_impl::DB_PER_DIV_DOWN);
		break;
	case Qt::Key_Right:
		this->d_block->execute_ui_action(qt_sink_c_impl::DB_PER_DIV_UP);
		break;
	case Qt::Key_Z:
		this->d_block->execute_ui_action(qt_sink_c_impl::ZOOM_TOGGLE);
		break;
	case Qt::Key_W:
		this->d_block->execute_ui_action(qt_sink_c_impl::ZOOM_WIDTH_UP);
		break;
	case Qt::Key_S:
		this->d_block->execute_ui_action(qt_sink_c_impl::ZOOM_WIDTH_DOWN);
		break;
	case Qt::Key_D:
		this->d_block->execute_ui_action(qt_sink_c_impl::ZOOM_CENTER_UP);
		break;
	case Qt::Key_A:
		this->d_block->execute_ui_action(qt_sink_c_impl::ZOOM_CENTER_DOWN);
		break;
	case Qt::Key_Q:
		this->d_block->execute_ui_action(qt_sink_c_impl::RATIO_UP);
		break;
	case Qt::Key_E:
		this->d_block->execute_ui_action(qt_sink_c_impl::RATIO_DOWN);
		break;
	case Qt::Key_Space:
		this->d_block->execute_ui_action(qt_sink_c_impl::FREEZE_TOGGLE);
		break;
	}
}

void
QGLSurface::mousePressEvent(QMouseEvent *me)
{
	int x, y;

	if (me->type() != QEvent::MouseButtonDblClick)
		return;

	qreal dpr = this->devicePixelRatioF();

	x = qRound(me->x() * dpr);
	y = qRound((this->size().height() - me->y() - 1) * dpr);

	this->d_block->execute_mouse_action(qt_sink_c_impl::CLICK, x, y);
}


void
QGLSurface::grabContext()
{
	/*
	 * Since the widget isn't made native together with its ancestors, its
	 * native window (and thus its GL context) may only exist once the
	 * toplevel window has been created, i.e. when it's first shown.
	 * Wait for that (but not forever).
	 */
	for (int i=0; i<200; i++) {
		bool ok = false;

		QMetaObject::invokeMethod(
			this,
			"giveContext",
			Qt::BlockingQueuedConnection,
			Q_RETURN_ARG(bool, ok),
			Q_ARG(QThread*, QThread::currentThread())
		);

		if (ok)
			break;

		QThread::msleep(50);
	}

	this->makeCurrent();
}

void
QGLSurface::releaseContext()
{
	this->doneCurrent();
	this->context()->moveToThread(this->d_gui_thread);
}

bool
QGLSurface::giveContext(QThread *thread)
{
	if (!this->context() || !this->context()->isValid()) {
		/* QGLWidget doesn't re-create its context when its native
		 * window is re-created (e.g. when it's re-parented) */
		if (!this->windowHandle())
			return false;

		bool was_visible = this->isVisible();

		this->setContext(new QGLContext(this->format()));

		/* (setContext hides the widget) */
		if (was_visible && !this->isVisible())
			this->show();

		if (!this->context() || !this->context()->isValid())
			return false;
	}

	this->doneCurrent();
	this->context()->moveToThread(thread);
	return true;
}

  } /* namespace fosphor */
} /* namespace gr */
