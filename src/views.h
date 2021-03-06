/* PDFTalk - PDF presentation tool
 * Copyright (C) 2016 - 2018 Francois Gindraud
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <QLabel>
#include <QPixmap>
#include <QWidget>

#include "controller.h"
#include "render.h"
class PageInfo;
namespace Action {
class Base;
}

/* This widget will show a PDF page (using a QLabel).
 * It is shown maximized (keeping aspect ratio), and centered.
 *
 * The current pixmap is indicated by a Render::Info structure.
 * This struct indicates which page is shown, and at which rendered size.
 *
 * Changes of current presentation page by the controller will trigger change_current_page ().
 * A request for a render is then sent to the rendering system.
 * The rendering system may never answer (size too small, etc).
 * The viewer will not display anything until the current page is changed.
 *
 * Requests for Pixmaps will go through the Rendering system.
 * The rendering system will broadcast request answers: receive_pixmap must filter incoming pixmaps.
 *
 * This widget also catches click events and will activate the page actions accordingly.
 */
class PageViewer : public QLabel {
	Q_OBJECT

private:
	ViewRole role_;                          // Selected role of this viewer
	const PageInfo * current_page_{nullptr}; // Current page of presentation
	Render::Info current_render_{};          // Current rendered page (requested or shown).
	bool requested_a_pixmap_{false};         // Did we request a render ?

public:
	explicit PageViewer (const ViewRole & role, QWidget * parent = nullptr);

	// Layouting info
	int heightForWidth (int w) const Q_DECL_FINAL;
	QSize sizeHint () const Q_DECL_FINAL;

	void resizeEvent (QResizeEvent *) Q_DECL_FINAL;
	void mouseReleaseEvent (QMouseEvent * event) Q_DECL_FINAL;

signals:
	void action_activated (const Action::Base * action);
	void request_render (Render::Request request);

public slots:
	void change_current_page (const PageInfo * new_current_page, RedrawCause cause);
	void receive_pixmap (const Render::Info & render_info, QPixmap pixmap);

private:
	void update_label (RedrawCause cause);
};

// Just one PageViewer, but also set a black background.
class PresentationView : public PageViewer {
	Q_OBJECT

public:
	explicit PresentationView (QWidget * parent = nullptr);
};

/* Presenter view.
 * Contains multiple PageViewers: current page, next slide, transitions if applicable.
 * Also show the timer, annotations, slide numbering.
 */
class PresenterView : public QWidget {
	Q_OBJECT

private:
	static constexpr qreal bottom_bar_text_point_size_factor = 2.0;

	const int nb_slides_;
	PageViewer * current_page_;
	PageViewer * previous_transition_page_;
	PageViewer * next_transition_page_;
	PageViewer * next_slide_first_page_;
	QLabel * annotations_;
	QLabel * slide_number_label_;
	QLabel * timer_label_;

public:
	explicit PresenterView (int nb_slides, QWidget * parent = nullptr);

	PageViewer * current_page_viewer () const { return current_page_; }
	PageViewer * next_slide_first_page_viewer () const { return next_slide_first_page_; }
	PageViewer * next_transition_page_viewer () const { return next_transition_page_; }
	PageViewer * previous_transition_page_viewer () const { return previous_transition_page_; }

public slots:
	void change_time (bool paused, const QString & new_time_text);
	void change_slide_info (const PageInfo * new_current_page);
};
