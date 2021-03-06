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

#include <utility>

#include <QByteArray>
#include <QCache>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QRunnable>

#include "render.h"

/* Internal header of the rendering system.
 * Header is required for moc to process Task/SystemPrivate classes.
 *
 * My analysis of PDFpc rendering strategy:
 * PDFpc prerenders pages and then compress the bitmap data.
 * When displaying, it thus needs to uncompress the data and recreate a "pixmap" to display.
 * No idea which pixmap caching is done, or how pixmap are resized for the different uses.
 *
 * In PDFTalk, window sizes are expected to change from program launch to presentation running.
 * No total prerendering is done.
 * Instead we use a LRU cache (bounded by a memory usage) of renders (indexed by page x size).
 * Rendering is done on demand (when pages are requested).
 * When a page is rendered (QImage), we store a qCompressed version in the cache (Compressed).
 * Page requests are fulfilled from the Compressed if available, or from a render.
 *
 * Pre rendering is delegated to a PrefetchStrategy class.
 * This class decides which pages to render based on the context from a Request.
 */
namespace Render {

// Stores data for a Compressed render
struct Compressed {
	QByteArray data;
	QSize size;
	int bytes_per_line;
	QImage::Format image_format;
};

/* Renders the page at the selected size.
 * Returns both the pixmap and a Compressed version.
 * The pixmap can be given to the requesting view.
 * The Compressed version can be stored in the render cache.
 *
 * Compressed renders are transmitted as owning raw pointers.
 * Signals cannot handle unique_ptr<Compressed> (move only unsupported).
 * And QCache requires an 'operator new' allocated object.
 */
std::pair<Compressed *, QPixmap> make_render (const Info & render_info);

/* Recreate a pixmap from a Compressed render.
 */
QPixmap make_pixmap_from_compressed_render (const Compressed & render);

// "Render a page" task for QThreadPool.
class Task : public QObject, public QRunnable {
	Q_OBJECT

private:
	const Info render_info_;

public:
	Task (const Info & render_info) : render_info_ (render_info) {}

signals:
	// "Render::Info" as Qt is not very namespace friendly
	void finished_rendering (Render::Info render_info, Compressed * compressed, QPixmap pixmap);

public:
	void run () Q_DECL_FINAL {
		auto result = make_render (render_info_);
		emit finished_rendering (render_info_, result.first, result.second);
	}
};

/* Caching system (internals).
 * Stores compressed renders in a cache to avoid rerendering stuff later.
 * Rendering is done through Tasks in a QThreadPool.
 *
 * Render requests arrive at request_render slot.
 * They are either served from the cache, or a render is launched.
 * In any case, prefetch renders are launched.
 *
 * Ongoing renders (render tasks) can be requested or prefetch.
 * Requested renders will emit a signal, as views requested them.
 * Prefetch renders emit no signal, and only update the cache.
 * If a render is requested while it is running, its status is updated to requested.
 * being_rendered tracks running renders, preventing double rendering and keeping their status.
 */
class SystemPrivate : public QObject {
	Q_OBJECT

private:
	System * parent_;
	QCache<Info, Compressed> cache_;

	enum class RenderType { Requested, Prefetch };
	QHash<Info, RenderType> being_rendered_;

	PrefetchStrategy * prefetch_strategy_;
	std::function<void(const Info &)> prefetch_render_lambda_; // for PrefetchStrategy, cached

public:
	SystemPrivate (int cache_size_bytes, PrefetchStrategy * strategy, System * parent);
	~SystemPrivate ();

	void request_render (const Request & request);

private slots:
	// "Render::Info" as Qt is not very namespace friendly
	void rendering_finished (Render::Info render_info, Compressed * compressed, QPixmap pixmap);

private:
	void perform_render (const Info & render_info, RenderType type);
};

/* Prefetch strategy interface.
 * Has a name for commandline identification.
 * Strategies must implement the prefetch method.
 * The context determines which pages will be pre rendered using pre_render.
 * pre_render should do nothing if the render is cached.
 */
class PrefetchStrategy {
private:
	QString name_;

public:
	PrefetchStrategy (const QString & name);
	virtual ~PrefetchStrategy () = default;
	const QString & name () const noexcept { return name_; }
	virtual void prefetch (const Request & context,
	                       const std::function<void(const Info &)> & request_render) = 0;
};
} // namespace Render
