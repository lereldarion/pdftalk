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
#include <QFont>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPalette>
#include <QSizeF>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "document.h"
#include "views.h"

// PageViewer

PageViewer::PageViewer (const ViewRole & role, QWidget * parent) : QLabel (parent), role_ (role) {
	setScaledContents (false);
	setAlignment (Qt::AlignCenter);
	setMinimumSize (1, 1); // To prevent nil QLabel when no pixmap is available
	QSizePolicy policy{QSizePolicy::Expanding, QSizePolicy::Expanding};
	policy.setHeightForWidth (true);
	setSizePolicy (policy);
}

int PageViewer::heightForWidth (int w) const {
	if (current_render_.isNull ()) {
		return QLabel::heightForWidth (w);
	} else {
		return current_render_.page ()->height_for_width_ratio () * w;
	}
}
QSize PageViewer::sizeHint () const {
	return {width (), heightForWidth (width ())};
}

void PageViewer::resizeEvent (QResizeEvent *) {
	update_label (RedrawCause::Resize);
}
void PageViewer::mouseReleaseEvent (QMouseEvent * event) {
	if (event->button () == Qt::LeftButton && !size ().isEmpty () && !current_render_.isNull ()) {
		// Determine pixmap position (centered)
		auto label_size = size ();
		auto pixmap_size = current_render_.size ();
		auto pixmap_offset_in_label = (label_size - pixmap_size) / 2;
		// Click position in pixmap
		auto click_pos_01 =
		    QPointF (static_cast<qreal> (event->x () - pixmap_offset_in_label.width ()) /
		                 static_cast<qreal> (pixmap_size.width ()),
		             static_cast<qreal> (event->y () - pixmap_offset_in_label.height ()) /
		                 static_cast<qreal> (pixmap_size.height ()));
		auto * action = current_render_.page ()->on_click (click_pos_01);
		if (action != nullptr)
			emit action_activated (action);
	}
}

void PageViewer::change_current_page (const PageInfo * new_current_page, RedrawCause cause) {
	current_page_ = new_current_page;
	update_label (cause);
}
void PageViewer::receive_pixmap (const Render::Info & render_info, QPixmap pixmap) {
	// Filter to only use the requested pixmaps
	if (requested_a_pixmap_ && render_info == current_render_) {
		requested_a_pixmap_ = false;
		setPixmap (pixmap);
	}
}

void PageViewer::update_label (RedrawCause cause) {
	auto request = Render::Request{current_page_, size (), role_, cause};
	auto new_render = request.requested_render ();
	if (new_render != current_render_) {
		current_render_ = new_render;
		clear (); // Remove old pixmap
		if (!current_render_.isNull ()) {
			requested_a_pixmap_ = true;
			emit request_render (request);
		}
	}
}

// PresentationView

PresentationView::PresentationView (QWidget * parent)
    : PageViewer (ViewRole::CurrentPublic, parent) {
	// Title
	setWindowTitle (tr ("Presentation screen"));
	// Black background
	QPalette p (palette ());
	p.setColor (QPalette::Window, Qt::black);
	setPalette (p);
	setAutoFillBackground (true);
	// Debug identification
	setObjectName ("presentation/current");
}

// PresenterView

PresenterView::PresenterView (int nb_slides, QWidget * parent)
    : QWidget (parent), nb_slides_ (nb_slides) {
	// Title
	setWindowTitle (tr ("Presenter screen"));
	// Black background, white text in childrens
	QPalette p (palette ());
	p.setColor (QPalette::Window, Qt::black);
	p.setColor (QPalette::WindowText, Qt::white);
	setPalette (p);
	setAutoFillBackground (true);

	// View structure
	auto * window_structure = new QVBoxLayout;
	setLayout (window_structure);
	{
		auto * slide_panels = new QHBoxLayout;
		window_structure->addLayout (slide_panels, 1);
		{
			// Current slide preview
			auto * current_slide_panel = new QVBoxLayout;
			slide_panels->addLayout (current_slide_panel, 6); // 60% screen width

			current_page_ = new PageViewer (ViewRole::CurrentPresenter);
			current_page_->setObjectName ("presenter/current");
			current_slide_panel->addWidget (current_page_, 7); // 70% screen height

			auto * transition_box = new QHBoxLayout;
			current_slide_panel->addLayout (transition_box, 3); // 30% screen height
			{
				previous_transition_page_ = new PageViewer (ViewRole::PrevTransition);
				previous_transition_page_->setObjectName ("presenter/prev_transition");
				transition_box->addWidget (previous_transition_page_);

				transition_box->addStretch ();

				next_transition_page_ = new PageViewer (ViewRole::NextTransition);
				next_transition_page_->setObjectName ("presenter/next_transition");
				transition_box->addWidget (next_transition_page_);
			}

			current_slide_panel->addStretch (); // Pad
		}
		{
			// Next slide preview, and annotations
			auto * next_slide_and_comment_panel = new QVBoxLayout;
			slide_panels->addLayout (next_slide_and_comment_panel, 4); // 40% screen width

			next_slide_first_page_ = new PageViewer (ViewRole::NextSlide);
			next_slide_first_page_->setObjectName ("presenter/next_slide");
			next_slide_and_comment_panel->addWidget (next_slide_first_page_);

			annotations_ = new QLabel;
			annotations_->setWordWrap (true);
			annotations_->setTextFormat (Qt::PlainText);
			// TODO Possible improvements:
			// - font size a bit larger
			// - margins between lines (non-wordwrapped ones)
			next_slide_and_comment_panel->addWidget (annotations_);

			next_slide_and_comment_panel->addStretch (); // Pad
		}
	}
	{
		// Bottom bar with slide number and time
		// Set font as 2 times bigger than normal
		auto * bottom_bar = new QHBoxLayout;
		window_structure->addLayout (bottom_bar);
		{
			slide_number_label_ = new QLabel;
			slide_number_label_->setAlignment (Qt::AlignCenter);
			slide_number_label_->setTextFormat (Qt::PlainText);
			QFont f (slide_number_label_->font ());
			f.setPointSizeF (bottom_bar_text_point_size_factor * f.pointSizeF ());
			slide_number_label_->setFont (f);
			bottom_bar->addWidget (slide_number_label_);
		}
		{
			timer_label_ = new QLabel;
			timer_label_->setAlignment (Qt::AlignCenter);
			timer_label_->setTextFormat (Qt::PlainText);
			QFont f (timer_label_->font ());
			f.setPointSizeF (bottom_bar_text_point_size_factor * f.pointSizeF ());
			timer_label_->setFont (f);
			bottom_bar->addWidget (timer_label_);
		}
	}
}

void PresenterView::change_time (bool paused, const QString & new_time_text) {
	// Set text as colored if paused
	auto color = Qt::white;
	if (paused)
		color = Qt::cyan;
	QPalette p (timer_label_->palette ());
	p.setColor (QPalette::WindowText, color);
	timer_label_->setPalette (p);
	timer_label_->setText (new_time_text);
}
void PresenterView::change_slide_info (const PageInfo * new_current_page) {
	Q_ASSERT (new_current_page != nullptr);
	auto * slide = new_current_page->slide ();
	slide_number_label_->setText (tr ("%1/%2").arg (slide->index () + 1).arg (nb_slides_));
	annotations_->setText (slide->annotations ());
}
