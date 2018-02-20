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

#include "controller.h"
class PageInfo;

#include <QDebug>
#include <QPixmap>
#include <QSize>
#include <functional>

/* Conversion between size str and integer size, with suffix support.
 * ("10k" <-> 10000)
 *
 * string_to_size_in_bytes returns a negative value on error.
 */
QString size_in_bytes_to_string (int size);
int string_to_size_in_bytes (QString size_str);

namespace Render {
class SystemPrivate;

/* Info represent a render metadata.
 * It is composed of a render size, and the selected page.
 * A "null" render represents invalid metadata (no page / zero size).
 *
 * The Info constructor accept any size: it will be shrunk to the biggest fitting render size.
 * Info is comparable / hashable to enable use as a hash table key (render system cache).
 */
class Info {
private:
	const PageInfo * page_{nullptr};
	QSize size_{};

public:
	Info () = default;
	Info (const PageInfo * p, const QSize & box);

	const PageInfo * page () const noexcept { return page_; }
	const QSize & size () const noexcept { return size_; }
	bool isNull () const noexcept { return page () == nullptr || size ().isNull (); }
};
bool operator== (const Info & a, const Info & b);
bool operator!= (const Info & a, const Info & b);
uint qHash (const Info & info, uint seed = 0);
QDebug operator<< (QDebug d, const Info & render_info);

/* Represent a render request comming from one of the views.
 * A view will request a render of a specific page, to fit within the view space.
 * A request must have a valid render info.
 *
 * The request will contain the actual render info (Info).
 * Additional informations for prefetching: current_page, box size, render role & cause.
 */
class Request {
private:
	Info requested_render_{};

	// Info used for prefetching
	const PageInfo * current_page_{nullptr};
	QSize box_size_{};
	ViewRole role_{ViewRole::Unknown};
	RedrawCause cause_{RedrawCause::Unknown};

public:
	Request () = default; // Required by Qt Moc, should not be used otherwise
	Request (const Info & requested_render, const PageInfo * current_page, const QSize & box,
	         ViewRole role, RedrawCause cause);

	const Info & requested_render () const noexcept { return requested_render_; }
	const PageInfo * current_page () const noexcept { return current_page_; }
	const QSize & box_size () const noexcept { return box_size_; }
	ViewRole role () const noexcept { return role_; }
	RedrawCause cause () const noexcept { return cause_; }
};

/* Prefetch strategy interface.
 * Has a name for commandline identification.
 * Strategies must implement the prefetch method.
 * The context determines which pages will be pre rendered using launch_render.
 */
class PrefetchStrategy {
private:
	QString name_;

public:
	PrefetchStrategy (const QString & name);
	virtual ~PrefetchStrategy () = default;
	const QString & name () const noexcept { return name_; }
	virtual void prefetch (const Request & context,
	                       const std::function<void(const Info &)> & launch_render) = 0;
};

/* Global rendering system.
 * Classes (viewers) can request a render by signaling request_render().
 * After some time, new_render will return the requested pixmap.
 * The new pixmap is broadcasted to all views; only requesting views will actually update.
 *
 * Internally, the cost of rendering is reduced by caching (see render_internal.h).
 * Additionally, the pages next to the current one are pre-rendered.
 * 'cache_size_bytes' sets the size of the cache in bytes.
 * 'strategy' defines the prefetch strategy, it can be null (no prefetch).
 */
class System : public QObject {
	Q_OBJECT

private:
	SystemPrivate * d_; // Cleanup is done through the QObject ownership tree

public:
	System (int cache_size_bytes, PrefetchStrategy * strategy);

signals:
	void new_render (const Info & render_info, QPixmap render_data);

public slots:
	void request_render (const Request & request);
};
} // namespace Render

Q_DECLARE_METATYPE (Render::Info);
Q_DECLARE_METATYPE (Render::Request);
