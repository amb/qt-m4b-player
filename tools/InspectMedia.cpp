#include "MetadataReader.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto args = app.arguments();
    args.removeFirst();
    const bool captions = args.removeAll("--captions") > 0;
    if (args.size() != 1) {
        std::cerr << "Usage: media-inspect [--captions] /path/to/book.m4b\n";
        return 2;
    }
    QElapsedTimer elapsed;
    elapsed.start();
    const auto data = MetadataReader::read(args.first(), std::make_shared<std::atomic_bool>(false), captions);
    if (!data.error.isEmpty()) {
        std::cerr << data.error.toStdString() << '\n';
        return 1;
    }
    QJsonArray chapters;
    for (const auto &chapter : data.chapters)
        chapters.push_back(QJsonObject{{"title", chapter.title}, {"start", chapter.start}, {"end", chapter.end}});
    const auto summary = QJsonObject{{"title", data.title}, {"author", data.author},
        {"duration", data.duration}, {"fingerprint", data.fingerprint}, {"chapters", chapters},
        {"cover_bytes", data.cover.size()}, {"captions_indexed", captions},
        {"caption_count", data.captions.size()}, {"elapsed_ms", elapsed.elapsed()}};
    std::cout << QJsonDocument(summary).toJson().constData();
}
