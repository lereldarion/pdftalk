/* PDFTalk - PDF presentation tool
 * Copyright (C) 2016 Francois Gindraud
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
#include "cache.h"
#include "controller.h"
#include "views.h"
#include "window.h"

#include <QApplication>
#include <QCommandLineParser>

int main (int argc, char * argv[]) {
	QApplication app (argc, argv);
	QCoreApplication::setApplicationName ("pdftalk");
#define XSTR(x) #x
#define STR(x) XSTR (x)
	QCoreApplication::setApplicationVersion (STR (PDFTALK_VERSION));
#undef STR
#undef XSTR
	QApplication::setApplicationDisplayName ("PDFTalk");

	qRegisterMetaType<CompressedRender> ();

	QCommandLineParser parser;
	parser.setApplicationDescription ("PDF presentation tool");
	parser.addHelpOption ();
	parser.addPositionalArgument ("pdf_file", QApplication::translate ("main", "PDF file to open"));
	parser.process (app);

	auto arguments = parser.positionalArguments ();
	if (arguments.size () != 1) {
		parser.showHelp (EXIT_FAILURE);
		Q_UNREACHABLE ();
	}

	const Document document (arguments[0]);
	Controller control (document);
	RenderCache cache (document);

	// Setup windows
	auto presentation_view = new PresentationView;
	auto presenter_view = new PresenterView (document.nb_slides ());
	add_shortcuts_to_widget (control, presentation_view);
	add_shortcuts_to_widget (control, presenter_view);

	// Link viewers to controller
	QObject::connect (&control, &Controller::slide_changed, presenter_view,
	                  &PresenterView::change_slide);
	QObject::connect (&control, &Controller::time_changed, presenter_view,
	                  &PresenterView::change_time);
	QObject::connect (&control, &Controller::annotations_changed, presenter_view,
	                  &PresenterView::change_annotations);

	QObject::connect (&control, &Controller::current_page_changed, presentation_view,
	                  &PageViewer::change_page);
	QObject::connect (&control, &Controller::current_page_changed,
	                  presenter_view->current_page_viewer (), &PageViewer::change_page);
	QObject::connect (&control, &Controller::next_slide_first_page_changed,
	                  presenter_view->next_slide_first_page_viewer (), &PageViewer::change_page);
	QObject::connect (&control, &Controller::next_transition_page_changed,
	                  presenter_view->next_transition_page_viewer (), &PageViewer::change_page);
	QObject::connect (&control, &Controller::previous_transition_page_changed,
	                  presenter_view->previous_transition_page_viewer (), &PageViewer::change_page);

	// Link viewers to caching system
	PageViewer * viewers[] = {presentation_view, presenter_view->current_page_viewer (),
	                          presenter_view->next_slide_first_page_viewer (),
	                          presenter_view->next_transition_page_viewer (),
	                          presenter_view->previous_transition_page_viewer ()};
	for (auto v : viewers) {
		QObject::connect (v, &PageViewer::request_pixmap, &cache, &RenderCache::request_page);
		QObject::connect (&cache, &RenderCache::new_pixmap, v, &PageViewer::receive_pixmap);
	}

	// Setup window swapping system
	WindowShifter windows{presentation_view, presenter_view};

	// Initialise timer text in interface
	control.timer_reset (); // TODO reset controller to give first page orders
	return app.exec ();
}
