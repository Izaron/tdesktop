#include "url_shortener.h"
#include <base/qthelp_url.h>

namespace {

	constexpr auto kShortenerApiUrl = "https://clck.ru/--?url=";

	QString MakeShortneredApiUrl(QStringView prevUrl) {
		return kShortenerApiUrl + prevUrl.toString();
	}

	QVector<QPair<int, int>> MakeUrlPos(QStringView text) {
		QVector<QPair<int, int>> result;
		int offset = 0;
		while (true) {
			const auto urlMatch = qthelp::RegExpHttpUrl().match(text, offset);
			if (!urlMatch.hasMatch()) {
				break;
			}
			result.push_back({ urlMatch.capturedStart(), urlMatch.capturedEnd() });
			offset = urlMatch.capturedEnd();
		}
		return result;
	}

} // namespace

UrlShortener::UrlShortener(TextWithTags textWithTags)
	: _textWithTags(std::move(textWithTags))
	, _urlPos(MakeUrlPos(_textWithTags.text))
{
	QObject::connect(&_mgr, SIGNAL(finished(QNetworkReply*)), this, SLOT(onNetworkReply(QNetworkReply*)));
}

bool UrlShortener::launchRequests() {
	if (_urlPos.empty()) {
		// there is nothing to do
		done();
		return false;
	}

	auto isUrlUsed = [usedUrls = QSet<QStringView>()](QStringView url) mutable {
		auto oldSize = usedUrls.size();
		usedUrls.insert(url);
		return oldSize == usedUrls.size();
	};

	for (const auto& pos : _urlPos) {
		const QString prevUrl = _textWithTags.text.mid(pos.first, pos.second - pos.first);
		if (isUrlUsed(prevUrl)) {
			continue;
		}
		const QNetworkRequest request(QUrl(MakeShortneredApiUrl(prevUrl)));
		_requestToPrevUrl.insert(_mgr.get(request), prevUrl);
	}

	return true;
}

TextWithTags&& UrlShortener::get()&& {
	return std::move(_textWithTags);
}

void UrlShortener::rewriteText() {
	auto& text = _textWithTags.text;
	auto& tags = _textWithTags.tags;

	const auto shiftUrlsAndTags = [&](int begin, int offset) {
		// shift urls
		for (auto& pos : _urlPos) {
			if (pos.first > begin) {
				pos.first += offset;
				pos.second += offset;
			}
		}

		// shift tags
		for (auto& tag : tags) {
			if (tag.offset > begin) {
				tag.offset += offset;
			}
		}
	};

	for (const auto& pos : _urlPos) {
		// find the new url
		const QString prevUrl = text.mid(pos.first, pos.second - pos.first);
		const QString& newUrl = _prevUrlToNewUrl.find(prevUrl).value();

		// replace the old url with the new url
		text = text.replace(pos.first, pos.second - pos.first, newUrl);

		// shift positions of other urls and tags
		shiftUrlsAndTags(pos.first, newUrl.size() - pos.second + pos.first);
	}
}

void UrlShortener::onNetworkReply(QNetworkReply* reply) {
	auto iter = _requestToPrevUrl.find(reply);
	_prevUrlToNewUrl.insert(iter.value(), reply->error() ? "<ERROR>" : reply->readAll());
	_requestToPrevUrl.erase(iter);

	if (_requestToPrevUrl.empty()) {
		rewriteText();
		done();
	}
}