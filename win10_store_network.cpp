#include "win10_store_network.h"

#include <cstdlib>
#include <rapidxml.hpp>
#include <rapidxml_print.hpp>
#include <sstream>
#include <playapi/util/http.h>
#include <chrono>

using namespace rapidxml;

const char* const Win10StoreNetwork::NAMESPACE_SOAP = "http://www.w3.org/2003/05/soap-envelope";
const char* const Win10StoreNetwork::NAMESPACE_ADDRESSING = "http://www.w3.org/2005/08/addressing";
const char* const Win10StoreNetwork::NAMESPACE_WSSECURITY_SECEXT = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd";
const char* const Win10StoreNetwork::NAMESPACE_WSSECURITY_UTILITY = "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd";
const char* const Win10StoreNetwork::NAMESPACE_WU_AUTHORIZATION = "http://schemas.microsoft.com/msus/2014/10/WindowsUpdateAuthorization";

const char* const Win10StoreNetwork::MINECRAFT_APP_ID = "d25480ca-36aa-46e6-b76b-39608d49558c";

const char* const Win10StoreNetwork::PRIMARY_URL = "https://fe3.delivery.mp.microsoft.com/ClientWebService/client.asmx";


void Win10StoreNetwork::buildCommonHeader(rapidxml::xml_document<> &doc, rapidxml::xml_node<> &header, const char* actionName) {
    auto action = doc.allocate_node(node_element, "a:Action", actionName);
    header.append_node(action);
    action->append_attribute(doc.allocate_attribute("s:mustUnderstand", "1"));
    auto messageId = doc.allocate_node(node_element, "a:MessageID", "urn:uuid:a68d4c75-ab85-4ca8-87db-136d281a2e28");
    header.append_node(messageId);
    auto to = doc.allocate_node(node_element, "a:To", "https://fe3.delivery.mp.microsoft.com/ClientWebService/client.asmx");
    header.append_node(to);
    to->append_attribute(doc.allocate_attribute("s:mustUnderstand", "1"));

    auto security = doc.allocate_node(node_element, "o:Security");
    header.append_node(security);
    security->append_attribute(doc.allocate_attribute("s:mustUnderstand", "1"));
    security->append_attribute(doc.allocate_attribute("xmlns:o", NAMESPACE_WSSECURITY_SECEXT));

    auto timestamp = doc.allocate_node(node_element, "Timestamp");
    security->append_node(timestamp);
    timestamp->append_attribute(doc.allocate_attribute("xmlns", NAMESPACE_WSSECURITY_UTILITY));
    timestamp->append_node(doc.allocate_node(node_element, "Created", "2019-01-01T00:00:00.000Z"));
    timestamp->append_node(doc.allocate_node(node_element, "Expires", "2100-01-01T00:00:00.000Z"));

    auto ticketsToken = doc.allocate_node(node_element, "wuws:WindowsUpdateTicketsToken");
    security->append_node(ticketsToken);
    ticketsToken->append_attribute(doc.allocate_attribute("wsu:id", "ClientMSA"));
    ticketsToken->append_attribute(doc.allocate_attribute("xmlns:wsu", NAMESPACE_WSSECURITY_UTILITY));
    ticketsToken->append_attribute(doc.allocate_attribute("xmlns:wuws", NAMESPACE_WU_AUTHORIZATION));

    if (userToken.size() > 0) {
        auto msaToken = doc.allocate_node(node_element, "TicketType");
        ticketsToken->append_node(msaToken);
        msaToken->append_attribute(doc.allocate_attribute("Name", "MSA"));
        msaToken->append_attribute(doc.allocate_attribute("Version", "1.0"));
        msaToken->append_attribute(doc.allocate_attribute("Policy", "MBI_SSL"));
        msaToken->append_node(doc.allocate_node(node_element, "User", userToken.c_str()));
    }

    auto aadToken = doc.allocate_node(node_element, "TicketType");
    ticketsToken->append_node(aadToken);
    aadToken->append_attribute(doc.allocate_attribute("Name", "AAD"));
    aadToken->append_attribute(doc.allocate_attribute("Version", "1.0"));
    aadToken->append_attribute(doc.allocate_attribute("Policy", "MBI_SSL"));
}

std::string Win10StoreNetwork::buildCookieRequest() {
    xml_document<> doc;
    auto envelope = doc.allocate_node(node_element, "s:Envelope");
    doc.append_node(envelope);
    envelope->append_attribute(doc.allocate_attribute("xmlns:a", NAMESPACE_ADDRESSING));
    envelope->append_attribute(doc.allocate_attribute("xmlns:s", NAMESPACE_SOAP));

    auto header = doc.allocate_node(node_element, "s:Header");
    envelope->append_node(header);

    buildCommonHeader(doc, *header, "http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService/GetCookie");

    auto body = doc.allocate_node(node_element, "s:Body");
    envelope->append_node(body);

    auto request = doc.allocate_node(node_element, "GetCookie");
    body->append_node(request);
    request->append_attribute(doc.allocate_attribute("xmlns", "http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService"));

    request->append_node(doc.allocate_node(node_element, "lastChange", "2018-06-26T21:31:32.8018693Z"));
    char timeBuf[512];
    formatTime(timeBuf, sizeof(timeBuf), std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    request->append_node(doc.allocate_node(node_element, "currentTime", doc.allocate_string(timeBuf)));
    request->append_node(doc.allocate_node(node_element, "protocolVersion", "1.81"));

    std::stringstream ss;
    rapidxml::print_to_stream(ss, doc);
    return ss.str();
}

std::string Win10StoreNetwork::buildSyncRequest(CookieData const& cookieData) {
    xml_document<> doc;
    auto envelope = doc.allocate_node(node_element, "s:Envelope");
    doc.append_node(envelope);
    envelope->append_attribute(doc.allocate_attribute("xmlns:a", NAMESPACE_ADDRESSING));
    envelope->append_attribute(doc.allocate_attribute("xmlns:s", NAMESPACE_SOAP));

    auto header = doc.allocate_node(node_element, "s:Header");
    envelope->append_node(header);

    buildCommonHeader(doc, *header, "http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService/SyncUpdates");

    auto body = doc.allocate_node(node_element, "s:Body");
    envelope->append_node(body);

    auto request = doc.allocate_node(node_element, "SyncUpdates");
    body->append_node(request);
    request->append_attribute(doc.allocate_attribute("xmlns", "http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService"));

    auto cookie = doc.allocate_node(node_element, "cookie");
    request->append_node(cookie);
    cookie->append_node(doc.allocate_node(node_element, "Expiration", cookieData.expiration.c_str()));
    cookie->append_node(doc.allocate_node(node_element, "EncryptedData", cookieData.encryptedData.c_str()));

    auto params = doc.allocate_node(node_element, "parameters");
    request->append_node(params);

    params->append_node(doc.allocate_node(node_element, "ExpressQuery", "false"));
    buildInstalledNonLeafUpdateIDs(doc, *params);
    params->append_node(doc.allocate_node(node_element, "SkipSoftwareSync", "false"));
    params->append_node(doc.allocate_node(node_element, "NeedTwoGroupOutOfScopeUpdates", "true"));
    auto filterAppCategoryIds = doc.allocate_node(node_element, "FilterAppCategoryIds");
    params->append_node(filterAppCategoryIds);
    auto filterAppCatId = doc.allocate_node(node_element, "CategoryIdentifier");
    filterAppCategoryIds->append_node(filterAppCatId);
    filterAppCatId->append_node(doc.allocate_node(node_element, "Id", MINECRAFT_APP_ID));
    params->append_node(doc.allocate_node(node_element, "TreatAppCategoryIdsAsInstalled", "true"));
    params->append_node(doc.allocate_node(node_element, "AlsoPerformRegularSync", "false"));
    params->append_node(doc.allocate_node(node_element, "ComputerSpec", ""));
    auto extendedUpdateInfoParams = doc.allocate_node(node_element, "ExtendedUpdateInfoParameters");
    params->append_node(extendedUpdateInfoParams);
    auto xmlUpdateFragmentTypes = doc.allocate_node(node_element, "XmlUpdateFragmentTypes");
    extendedUpdateInfoParams->append_node(xmlUpdateFragmentTypes);
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "Extended"));
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "LocalizedProperties"));
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "Eula"));
    auto extendedUpdateLocales = doc.allocate_node(node_element, "Locales");
    extendedUpdateInfoParams->append_node(extendedUpdateLocales);
    extendedUpdateLocales->append_node(doc.allocate_node(node_element, "string", "en-US"));
    extendedUpdateLocales->append_node(doc.allocate_node(node_element, "string", "en"));

    auto clientPreferredLanguages = doc.allocate_node(node_element, "ClientPreferredLanguages");
    params->append_node(clientPreferredLanguages);
    clientPreferredLanguages->append_node(doc.allocate_node(node_element, "string", "en-US"));

    auto productsParameters = doc.allocate_node(node_element, "ProductsParameters");
    params->append_node(productsParameters);
    productsParameters->append_node(doc.allocate_node(node_element, "SyncCurrentVersionOnly", "false"));
    productsParameters->append_node(doc.allocate_node(node_element, "DeviceAttributes", "E:BranchReadinessLevel=CBB&DchuNvidiaGrfxExists=1&ProcessorIdentifier=Intel64%20Family%206%20Model%2063%20Stepping%202&CurrentBranch=rs4_release&DataVer_RS5=1942&FlightRing=Retail&AttrDataVer=57&InstallLanguage=en-US&DchuAmdGrfxExists=1&OSUILocale=en-US&InstallationType=Client&FlightingBranchName=&Version_RS5=10&UpgEx_RS5=Green&GStatus_RS5=2&OSSkuId=48&App=WU&InstallDate=1529700913&ProcessorManufacturer=GenuineIntel&AppVer=10.0.17134.471&OSArchitecture=AMD64&UpdateManagementGroup=2&IsDeviceRetailDemo=0&HidOverGattReg=C%3A%5CWINDOWS%5CSystem32%5CDriverStore%5CFileRepository%5Chidbthle.inf_amd64_467f181075371c89%5CMicrosoft.Bluetooth.Profiles.HidOverGatt.dll&IsFlightingEnabled=0&DchuIntelGrfxExists=1&TelemetryLevel=1&DefaultUserRegion=244&DeferFeatureUpdatePeriodInDays=365&Bios=Unknown&WuClientVer=10.0.17134.471&PausedFeatureStatus=1&Steam=URL%3Asteam%20protocol&Free=8to16&OSVersion=10.0.17134.472&DeviceFamily=Windows.Desktop"));
    productsParameters->append_node(doc.allocate_node(node_element, "CallerAttributes", "E:Interactive=1&IsSeeker=1&Acquisition=1&SheddingAware=1&Id=Acquisition%3BMicrosoft.WindowsStore_8wekyb3d8bbwe&"));
    productsParameters->append_node(doc.allocate_node(node_element, "Products"));

    std::stringstream ss;
    rapidxml::print_to_stream(ss, doc);
    return ss.str();
}

std::string Win10StoreNetwork::buildDownloadLinkRequest(std::string const &updateId, int revisionNumber) {
    xml_document<> doc;
    auto envelope = doc.allocate_node(node_element, "s:Envelope");
    doc.append_node(envelope);
    envelope->append_attribute(doc.allocate_attribute("xmlns:a", NAMESPACE_ADDRESSING));
    envelope->append_attribute(doc.allocate_attribute("xmlns:s", NAMESPACE_SOAP));

    auto header = doc.allocate_node(node_element, "s:Header");
    envelope->append_node(header);

    buildCommonHeader(doc, *header, "http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService/GetExtendedUpdateInfo2");

    auto body = doc.allocate_node(node_element, "s:Body");
    envelope->append_node(body);

    auto request = doc.allocate_node(node_element, "GetExtendedUpdateInfo2");
    body->append_node(request);
    request->append_attribute(doc.allocate_attribute("xmlns", "http://www.microsoft.com/SoftwareDistribution/Server/ClientWebService"));

    auto updateIds = doc.allocate_node(node_element, "updateIDs");
    request->append_node(updateIds);
    auto updateIdNode = doc.allocate_node(node_element, "UpdateIdentity");
    updateIds->append_node(updateIdNode);
    updateIdNode->append_node(doc.allocate_node(node_element, "UpdateID", updateId.c_str()));
    std::string revisionNumberStr = std::to_string(revisionNumber);
    updateIdNode->append_node(doc.allocate_node(node_element, "RevisionNumber", revisionNumberStr.c_str()));

    auto xmlUpdateFragmentTypes = doc.allocate_node(node_element, "infoTypes");
    request->append_node(xmlUpdateFragmentTypes);
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "FileUrl"));
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "FileDecryption"));
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "EsrpDecryptionInformation"));
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "PiecesHashUrl"));
    xmlUpdateFragmentTypes->append_node(doc.allocate_node(node_element, "XmlUpdateFragmentType", "BlockMapUrl"));

    request->append_node(doc.allocate_node(node_element, "deviceAttributes", "E:BranchReadinessLevel=CBB&DchuNvidiaGrfxExists=1&ProcessorIdentifier=Intel64%20Family%206%20Model%2063%20Stepping%202&CurrentBranch=rs4_release&DataVer_RS5=1942&FlightRing=Retail&AttrDataVer=57&InstallLanguage=en-US&DchuAmdGrfxExists=1&OSUILocale=en-US&InstallationType=Client&FlightingBranchName=&Version_RS5=10&UpgEx_RS5=Green&GStatus_RS5=2&OSSkuId=48&App=WU&InstallDate=1529700913&ProcessorManufacturer=GenuineIntel&AppVer=10.0.17134.471&OSArchitecture=AMD64&UpdateManagementGroup=2&IsDeviceRetailDemo=0&HidOverGattReg=C%3A%5CWINDOWS%5CSystem32%5CDriverStore%5CFileRepository%5Chidbthle.inf_amd64_467f181075371c89%5CMicrosoft.Bluetooth.Profiles.HidOverGatt.dll&IsFlightingEnabled=0&DchuIntelGrfxExists=1&TelemetryLevel=1&DefaultUserRegion=244&DeferFeatureUpdatePeriodInDays=365&Bios=Unknown&WuClientVer=10.0.17134.471&PausedFeatureStatus=1&Steam=URL%3Asteam%20protocol&Free=8to16&OSVersion=10.0.17134.472&DeviceFamily=Windows.Desktop"));

    std::stringstream ss;
    rapidxml::print_to_stream(ss, doc);
    return ss.str();
}

void Win10StoreNetwork::buildInstalledNonLeafUpdateIDs(rapidxml::xml_document<> &doc,
                                                       rapidxml::xml_node<> &paramsNode) {
    // Mostly random updates, took from my primary Windows installation + the detectoids for ARM
    int installedNonLeafUpdateIDs[] = {1, 2, 3, 11, 19, 2359974, 5169044, 8788830, 23110993, 23110994, 59830006,
                                       59830007, 59830008, 60484010, 62450018, 62450019, 62450020, 98959022, 98959023,
                                       98959024, 98959025, 98959026, 129905029, 130040030, 130040031, 130040032,
                                       130040033, 138372035, 138372036, 139536037, 158941041, 158941042, 158941043,
                                       158941044,
                                       // ARM
                                       133399034, 2359977
    };
    auto node = doc.allocate_node(node_element, "InstalledNonLeafUpdateIDs");
    paramsNode.append_node(node);
    char buf[32];
    for (int i : installedNonLeafUpdateIDs) {
        sprintf(buf, "%i", i);
        node->append_node(doc.allocate_node(node_element, "int", doc.allocate_string(buf)));
    }
}

void Win10StoreNetwork::formatTime(char* buf, size_t bufSize, time_t time) {
    struct tm tm;
    time_t tt = time;
    gmtime_r(&tt, &tm);
    strftime(buf, bufSize, "%FT%TZ", &tm);
}

size_t Win10StoreNetwork::httpOnWrite(char *ptr, size_t size, size_t nmemb, void *userdata) {
    ((std::string*) userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

void Win10StoreNetwork::doHttpRequest(const char *url, const char *data, std::string &ret) {
    printf("Request with body: %s\n", data);

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/soap+xml; charset=utf-8");
    headers = curl_slist_append(headers, "User-Agent: Windows-Update-Agent/10.0.10011.16384 Client-Protocol/1.81");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpOnWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    printf("Response: %s\n", ret.c_str());

    if (res != CURLE_OK)
        throw std::runtime_error("doHttpRequest: res not ok");
}

Win10StoreNetwork::CookieData Win10StoreNetwork::fetchCookie() {
    std::string request = buildCookieRequest();
    std::string ret;
    doHttpRequest(Win10StoreNetwork::PRIMARY_URL, request.c_str(), ret);
    xml_document<> doc;
    doc.parse<0>(&ret[0]);
    auto& envelope = firstNodeOrThrow(doc, "s:Envelope");
    auto& body = firstNodeOrThrow(envelope, "s:Body");
    auto& resp = firstNodeOrThrow(body, "GetCookieResponse");
    auto& res = firstNodeOrThrow(resp, "GetCookieResult");

    CookieData data;
    data.encryptedData = firstNodeOrThrow(res, "EncryptedData").value();
    data.expiration = firstNodeOrThrow(res, "Expiration").value();
    return data;
}

Win10StoreNetwork::SyncResult Win10StoreNetwork::syncVersion(CookieData const& cookie) {
    std::string request = buildSyncRequest(cookie);
    std::string ret;
    doHttpRequest(Win10StoreNetwork::PRIMARY_URL, request.c_str(), ret);
    xml_document<> doc;
    doc.parse<0>(&ret[0]);
    auto& envelope = firstNodeOrThrow(doc, "s:Envelope");
    auto& body = firstNodeOrThrow(envelope, "s:Body");
    auto& resp = firstNodeOrThrow(body, "SyncUpdatesResponse");
    auto& res = firstNodeOrThrow(resp, "SyncUpdatesResult");
    auto& newUpdates = firstNodeOrThrow(res, "NewUpdates");
    SyncResult data;
    for (auto it = newUpdates.first_node("UpdateInfo"); it != nullptr; it = it->next_sibling("UpdateInfo")) {
        UpdateInfo info;
        info.serverId = firstNodeOrThrow(*it, "ID").value();
        info.addXmlInfo(firstNodeOrThrow(*it, "Xml").value()); // NOTE: destroys the node
        data.newUpdates.push_back(std::move(info));
    }
    auto newCookie = res.first_node("NewCookie");
    if (newCookie != nullptr) {
        data.newCookie.encryptedData = firstNodeOrThrow(*newCookie, "EncryptedData").value();
        data.newCookie.expiration = firstNodeOrThrow(*newCookie, "Expiration").value();
    }
    return data;
}

Win10StoreNetwork::DownloadLinkResult Win10StoreNetwork::getDownloadLink(
        std::string const &updateId, int revisionNumber) {
    std::string request = buildDownloadLinkRequest(updateId, revisionNumber);
    std::string ret;
    doHttpRequest(Win10StoreNetwork::PRIMARY_URL, request.c_str(), ret);
    xml_document<> doc;
    doc.parse<0>(&ret[0]);
    auto& envelope = firstNodeOrThrow(doc, "s:Envelope");
    auto& body = firstNodeOrThrow(envelope, "s:Body");
    auto& resp = firstNodeOrThrow(body, "GetExtendedUpdateInfo2Response");
    auto& res = firstNodeOrThrow(resp, "GetExtendedUpdateInfo2Result");
    auto& fileLocations = firstNodeOrThrow(res, "FileLocations");
    DownloadLinkResult data;
    for (auto it = fileLocations.first_node("FileLocation"); it != nullptr; it = it->next_sibling("FileLocation")) {
        FileLocation info;
        info.url = firstNodeOrThrow(*it, "Url").value();
        data.files.push_back(std::move(info));
    }
    return data;
}

void Win10StoreNetwork::UpdateInfo::addXmlInfo(char *val) {
    printf("%s\n", val);
    xml_document<> doc;
    doc.parse<0>(val);
    auto& identity = firstNodeOrThrow(doc, "UpdateIdentity");
    auto attr = identity.first_attribute("UpdateID");
    if (attr != nullptr)
        updateId = attr->value();
    auto applicability = doc.first_node("ApplicabilityRules");
    auto metadata = applicability != nullptr ? applicability->first_node("Metadata") : nullptr;
    auto metadataPkgAppx = metadata != nullptr ? metadata->first_node("AppxPackageMetadata") : nullptr;
    auto metadataAppx = metadataPkgAppx != nullptr ? metadataPkgAppx->first_node("AppxMetadata") : nullptr;
    attr = metadataAppx != nullptr ? metadataAppx->first_attribute("PackageMoniker") : nullptr;
    if (attr != nullptr)
        packageMoniker = attr->value();
}