#pragma once

#include <ui/text/text_entity.h>
#include <QtNetwork/qnetworkreply.h>

class UrlShortener final : public QObject {
	Q_OBJECT

public:
	UrlShortener(TextWithTags textWithTags);
	bool launchRequests();
	TextWithTags&& get()&&;

private:
	void rewriteText();

Q_SIGNALS:
	void done();

private Q_SLOTS:
	void onNetworkReply(QNetworkReply* reply);

private:
	TextWithTags _textWithTags;
	QNetworkAccessManager _mgr;
	QVector<QPair<int, int>> _urlPos;
	QMap<QNetworkReply*, QString> _requestToPrevUrl;
	QMap<QString, QString> _prevUrlToNewUrl;
};