#include "discord/webhook.hpp"

#include "discord/map_image.hpp"
#include "discord/payload.hpp"
#include "util.hpp"

#include <Windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace erstats {
namespace {

WebhookResult http_body(
    const ParsedWebhook& parsed,
    const wchar_t* method,
    const std::wstring& path,
    const std::wstring& content_type,
    const std::string& body) {
    WebhookResult result;
    HINTERNET session = WinHttpOpen(
        L"EldenRing-StatsShare/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        result.error = "WinHttpOpen failed";
        return result;
    }
    WinHttpSetTimeouts(session, 3000, 3000, 5000, 8000);

    HINTERNET connect = WinHttpConnect(
        session, parsed.host.c_str(), parsed.https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        result.error = "WinHttpConnect failed";
        return result;
    }

    const DWORD flags = parsed.https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(
        connect, method, path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.error = "WinHttpOpenRequest failed";
        return result;
    }

    const std::wstring headers = L"Content-Type: " + content_type + L"\r\n";
    const BOOL sent = WinHttpSendRequest(
        request, headers.c_str(), static_cast<DWORD>(-1L),
        reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
        static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        result.error = "WinHttp send/receive failed";
        return result;
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(
        request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
        &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    result.status = static_cast<int>(status);

    std::string response;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            break;
        }
        chunk.resize(read);
        response += chunk;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    result.ok = status >= 200 && status < 300;
    if (const auto id = extract_message_id(response)) {
        result.message_id = *id;
    }
    if (!result.ok) {
        result.error = "HTTP " + std::to_string(status);
    }
    return result;
}

std::string multipart_body(
    const std::string& json, const std::vector<uint8_t>& png, const std::string& boundary) {
    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"payload_json\"\r\n";
    body += "Content-Type: application/json\r\n\r\n";
    body += json;
    body += "\r\n--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"files[0]\"; filename=\"location.png\"\r\n";
    body += "Content-Type: image/png\r\n\r\n";
    body.append(reinterpret_cast<const char*>(png.data()), png.size());
    body += "\r\n--" + boundary + "--\r\n";
    return body;
}

WebhookResult send_payload(
    const ParsedWebhook& parsed,
    const wchar_t* method,
    const std::wstring& path,
    const std::string& json,
    const std::optional<std::vector<uint8_t>>& png) {
    if (png) {
        const std::string boundary = "----ERStatsShare7f3a9c";
        return http_body(
            parsed, method, path,
            L"multipart/form-data; boundary=" + utf8_to_wide(boundary),
            multipart_body(json, *png, boundary));
    }
    return http_body(parsed, method, path, L"application/json", json);
}

}  // namespace

WebhookResult post_or_edit_webhook(const Config& config, RunSnapshot& snapshot) {
    WebhookResult result;
    if (config.webhook_url.empty()) {
        result.error = "webhook url empty";
        return result;
    }
    const auto parsed = parse_webhook_url(config.webhook_url);
    if (!parsed) {
        result.error = "invalid webhook url";
        return result;
    }

    const auto map_png = render_location_map(snapshot);
    const std::string body = build_webhook_payload(snapshot, map_png.has_value());

    std::wstring path = parsed->path;
    if (path.find(L'?') == std::wstring::npos) {
        path += L"?wait=true";
    } else {
        path += L"&wait=true";
    }
    result = send_payload(*parsed, L"POST", path, body, map_png);
    if (result.ok && !result.message_id.empty()) {
        snapshot.discord_message_id = result.message_id;
    }
    return result;
}

}  // namespace erstats
