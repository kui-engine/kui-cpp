// Sample 23 - Document::printToPDF end-to-end demo and 50-page A4 benchmark.
// The example intentionally uses only the public C++ SDK surface so the same
// source builds unchanged against the C++ wrapper.
#include <kui/kui.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace kui;

namespace {

std::string buildDemoHtml(int pageCount) {
    std::string body;
    body.reserve(static_cast<std::size_t>(pageCount) * 1200);
    for (int i = 1; i <= pageCount; ++i) {
        body += "<section style='page-break-after:always'>";
        body += "<h2>Section " + std::to_string(i) + "</h2>";
        body +=
            "<p>This page exercises the public KUI PDF pipeline: DOM parsing, "
            "layout, pagination, display-list replay and SkPDF output.</p>"
            "<ul><li>Public Document API only</li>"
            "<li>A4 pagination with running page counters</li>"
            "<li>Shared source for the native SDK and C++ wrapper</li></ul>"
            "<table border='1' cellpadding='4' style='border-collapse:collapse;width:100%'>"
            "<thead><tr><th>Capability</th><th>Status</th></tr></thead><tbody>"
            "<tr><td>DOM and style</td><td>enabled</td></tr>"
            "<tr><td>Pagination</td><td>enabled</td></tr>"
            "<tr><td>PDF metadata</td><td>enabled</td></tr>"
            "</tbody></table></section>";
    }

    return
        "<!DOCTYPE html><html><head><style>"
        "@page{size:A4;margin:25mm 20mm 30mm 20mm;"
        "@top-left{content:'KUI printToPDF demo';font-size:9pt;color:#555}"
        "@top-right{content:'Sample 23';font-size:9pt;color:#555}"
        "@bottom-right{content:'Page ' counter(page) ' of ' counter(pages);font-size:10pt}}"
        "body{font-family:'Segoe UI',sans-serif;font-size:11pt;line-height:1.45;color:#1f2937;margin:0}"
        "section{padding:0 8px}h2{color:#2563eb;border-bottom:1px solid #e5e7eb}"
        "table{font-size:10pt;margin:8pt 0}th{background:#eff6ff;text-align:left}"
        "</style></head><body>" + body + "</body></html>";
}

template <class Clock = std::chrono::steady_clock>
double elapsedMs(typename Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

} // namespace

int main(int argc, char** argv) {
    constexpr int kPageCount = 50;
    const fs::path outputPath = argc >= 2
        ? fs::path(argv[1])
        : fs::temp_directory_path() / "kui_sample23.pdf";
    std::error_code error;
    fs::remove(outputPath, error);

    if (!Platform::init()) {
        std::fprintf(stderr, "Platform::init failed\n");
        return 1;
    }

    WindowConfig config;
    config.title = "KUI PDF export";
    config.width = 800;
    config.height = 600;
    config.visible = false;
    auto window = Window::create(config);
    if (!window) {
        std::fprintf(stderr, "Window::create failed\n");
        return 2;
    }

    auto* document = window->document();
    const auto html = buildDemoHtml(kPageCount);
    auto start = std::chrono::steady_clock::now();
    document->loadHTML(html);
    const double parseMs = elapsedMs(start);

    PDFOptions options;
    options.pageWidth_pt = 595.0f;
    options.pageHeight_pt = 842.0f;
    options.title = "KUI Sample 23 - printToPDF demo";
    options.author = "kui-samples";
    options.subject = "Public SDK PDF end-to-end demo";

    start = std::chrono::steady_clock::now();
    const bool written = document->printToPDF(outputPath.string(), options);
    const double pdfMs = elapsedMs(start);
    if (!written) {
        std::fprintf(stderr, "Document::printToPDF returned false\n");
        return 3;
    }

    const auto size = fs::file_size(outputPath, error);
    if (error || size == 0) {
        std::fprintf(stderr, "PDF output is missing or empty\n");
        return 4;
    }

    std::printf("KUI sample 23 - printToPDF demo\n");
    std::printf("  pages       : %d\n", kPageCount);
    std::printf("  output      : %s\n", outputPath.string().c_str());
    std::printf("  HTML parse  : %7.1f ms\n", parseMs);
    std::printf("  printToPDF  : %7.1f ms (%.2f ms/page)\n", pdfMs, pdfMs / kPageCount);
    std::printf("  file size   : %.1f KB\n", static_cast<double>(size) / 1024.0);
    std::printf("  status      : OK\n");
    if (pdfMs > 2000.0)
        std::printf("  WARNING: 2000 ms benchmark budget exceeded\n");
    return 0;
}
