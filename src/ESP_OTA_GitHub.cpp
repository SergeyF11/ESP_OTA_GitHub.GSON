/*
  ESP_OTA_GitHub.cpp - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/

#include "ESP_OTA_GitHub.h"

ESPOTAGitHub::ESPOTAGitHub( const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease) /*:
    _certStore(nullptr), _cert(nullptr) */
    {
        _init(user, repo,currentTag, binFile, preRelease);
//    _certStore = nullptr;
//    _cert = nullptr;
    // _user = user;
    // _repo = repo;
    // _currentTag = currentTag;
    // _binFile = binFile;
    // _preRelease = preRelease;
    // _lastError = NULL_STR;
    // _upgradeURL = NULL_STR;
    // _releaseTag = NULL_STR;
}
ESPOTAGitHub::ESPOTAGitHub(BearSSL::CertStore* certStore, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease) :
     _certStore(certStore)
{
    _init(user, repo,currentTag, binFile, preRelease);
    //_certStore = certStore;
    // _user = user;
    // _repo = repo;
    // _currentTag = currentTag;
    // _binFile = binFile;
    // _preRelease = preRelease;
    // _lastError = NULL_STR;
    // _upgradeURL = NULL_STR;
    // _releaseTag = NULL_STR;
}

ESPOTAGitHub::ESPOTAGitHub(BearSSL::X509List* certs, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease) :
    _cert(certs)
{
    _init(user, repo,currentTag, binFile, preRelease);
    _cert->append(GITHUB_CERTIFICATE_ROOT);
    _cert->append(GITHUB_CERTIFICATE_ROOT1);
    // _user = user;
    // _repo = repo;
    // _currentTag = currentTag;
    // _binFile = binFile;
    // _preRelease = preRelease;
    // _lastError = NULL_STR;
    // _upgradeURL = NULL_STR;
    // _releaseTag = NULL_STR;
}

static const char _http[] PROGMEM = "http://";
static const char _https[] PROGMEM = "https://";

bool ESPOTAGitHub::clean(){
		_lastError = NULL_STR; 
		_upgradeURL= NULL_STR; 
		_releaseTag= NULL_STR;
        return _lastError.reserve(0) + _upgradeURL.reserve(0) + _releaseTag.reserve(0);
}


/* Private methods */
urlDetails_t ESPOTAGitHub::_urlDetails(String url, urlDetails_t *urlDetailP = nullptr) {
    //String proto = "";
    if ( urlDetailP == nullptr){
        urlDetailP = new(urlDetails_t);
    }
    //int port = 80;
    if (url.startsWith( _http )){ //"http://")) {
        urlDetailP->proto = _http; //"http://";
        urlDetailP->port = 80;
        //url.replace( _http /*"http://"*/, "");
        url = url.substring(7);
    } else {
        urlDetailP->proto = _https; //"https://";
        urlDetailP->port = 443;
        //url.replace(_https /*"https://"*/, "");
        url = url.substring(8);
    }

    int firstSlash = url.indexOf('/');
    urlDetailP->host = url.substring(0, firstSlash);;
    urlDetailP->path = url.substring(firstSlash);;

    return *urlDetailP;
};


bool ESPOTAGitHub::_setClientSecure(BearSSL::WiFiClientSecure& c){

OTAdebugPrint("GitHub connection use ");

    if ( _certStore != nullptr ){
        c.setCertStore(_certStore);
    
        OTAdebugPrintln("certificate store");
    
    } else if ( _cert != nullptr ){
        c.setTrustAnchors( _cert );
        OTAdebugPrintln("X509 list");
    
    } else {
        c.setInsecure();
        OTAdebugPrintln("insecure connection");
    
    }
    return ! _useInsecureConnection();
}

bool ESPOTAGitHub::_resolveRedirects() {
    urlDetails_t splitURL = _urlDetails(_upgradeURL);
    String proto = splitURL.proto;
    String host = splitURL.host;
    int port = splitURL.port;
    String path = splitURL.path;
    bool isFinalURL = false;

    BearSSL::WiFiClientSecure client;
    _setClientSecure(client);

    // if ( _certStore == nullptr ){
    //     client.setInsecure();
    // } else {
    //     client.setCertStore(_certStore);
    // }

    while (!isFinalURL) {
        if (!client.connect(host, port)) {
            _lastError = F("Connection Failed.");
            return false;
        }

 
    client.print(F("GET "));
    client.print(path);
    client.print(F(" HTTP/1.1\r\n"));
    client.print(F("Host: ") ); client.print( host );
    client.print(F("\r\n"));
    client.print(F("User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n"));
    client.print(F("Connection: close\r\n\r\n"));

        // client.print(String("GET ") + path + " HTTP/1.1\r\n" +
        //     "Host: " + host + "\r\n" +
        //     "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n" +
        //     "Connection: close\r\n\r\n");

        while (client.connected()) {
            String response = client.readStringUntil('\n');
 
            if ( response.substring(0,9).equalsIgnoreCase( F("location:")) ){
            //if (response.startsWith("location: ") || response.startsWith("Location: ")) {
                isFinalURL = false;
                String location = response.substring(10, response.length() -1 );
                

                if (location.startsWith(_http) || location.startsWith(_https)) {
                    //absolute URL - separate host from path
                    urlDetails_t url = _urlDetails(location);
                    proto = url.proto;
                    host = url.host;
                    port = url.port;
                    path = url.path;
                } else {
					//relative URL - host is the same as before, location represents the new path.
                    path = location;
                }
                //leave the while loop so we don't set isFinalURL on the next line of the header.
                break;
            } else {
                //location header not present - this must not be a redirect. Treat this as the final address.
                isFinalURL = true;
            }
            if (response == "\r") {
                break;
            }
        }
        
    }
    client.stop();
    if(isFinalURL) {
        String finalURL = proto + host + path;
        _upgradeURL = finalURL;
    #if defined ESPOTAGitHub_DEBUG
        Serial.println( _upgradeURL );
    #endif
        return true;
    } else {
        _lastError = F("CONNECTION FAILED");
        return false;
    }
}

// Set time via NTP, as required for x.509 validation

void ESPOTAGitHub::_setClock() {
    if ( time(nullptr) > _likeRealTime ) return;

	configTime("GMT-0", GHOTA_NTP1, GHOTA_NTP2);  // UTC

	time_t now = time(nullptr);
	while (now < _likeRealTime) {
		yield();
        Serial.print(".");
		delay(500);
		now = time(nullptr);
	}
	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
}

/* Public methods */

bool ESPOTAGitHub::checkUpgrade() {
    _setClock(); // Clock needs to be set to perform certificate checks
    BearSSL::WiFiClientSecure client;
    _setClientSecure(client);

    if (!client.connect(GHOTA_HOST, GHOTA_PORT)) {
        _lastError = "Connection failed";
        return false;
    }

    client.print(F("GET /repos/"));
    //client.print(url);
    client.print( _user );
    client.print('/');
    client.print( _repo);
    client.print(F("/releases/latest"));
    
    client.print(F(" HTTP/1.1\r\n"));
    client.print(F("Host: ") ); client.print( GHOTA_HOST );
    client.print(F("\r\n"));
    client.print(F("User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n"));
    client.print(F("Connection: close\r\n\r\n"));
    client.setTimeout(GHOTA_TIMEOUT);

    OTArunStart;
    while (client.connected()) {
        String response = client.readStringUntil('\n');
        if (response == "\r") {
			break;
        }
    }
    OTAprintRunTime;

    OTArunStart;    
    String response = client.readString(); //Until('\n');
    OTAprintRunTime;

    gson::Parser doc;
    // while ( response.startsWith("{") && ! doc.parse(response.c_str(), response.length())){
    //     delay(10);
    //     Serial.println(response);
    //     response += client.readString();
    // }
    client.stop();
    bool result = false;
    if ( doc.parse(response.c_str(), response.length()) && doc.has("tag_name")) {
        _releaseTag = doc["tag_name"].toString();
        String release_name = doc["name"].toString();
        bool release_prerelease = doc["prerelease"].toBool();
        
        if (strcmp(_releaseTag.c_str(), _currentTag) != 0) {
            if (!_preRelease) {
                if (release_prerelease) {
                    _lastError = F("Latest release is a pre-release and GHOTA_ACCEPT_PRERELEASE is set to false.");
                    //return false;
                }
            }
            if ( doc.has("assets") && doc["assets"].isArray() ){ //&& doc["assets"].isArray() ){
                int i = 0;
                bool valid_asset = false;
                
                while(  doc["assets"][i].isObject() ){
                
                    String asset_type = doc["assets"][i]["content_type"].toString();
                    String asset_name = doc["assets"][i]["name"].toString();
                    String asset_url = doc["assets"][i]["browser_download_url"].toString();
                        
                    if (strcmp(asset_type.c_str(), GHOTA_CONTENT_TYPE) == 0 && strcmp(asset_name.c_str(), _binFile) == 0) {
                        _upgradeURL = asset_url;
                        valid_asset = true;
                        break;
                    } else {
                        valid_asset = false;
                    }
                    i++;
                }
                //doc.reset();
                if (valid_asset) {
                    result = true;
                    //return true;
                } else {
                    _lastError = F("No valid binary found for latest release.");
                    //return false;
                }
            } else {
                _lastError = F("Already running latest release.");
                //return false;
            }
        } else {
            _lastError = F("JSON didn't match expected structure. 'tag_name' missing.");
            //return false;
        }
    } else {
        _lastError = F("Failed to parse JSON."); // Error was: " + error.c_str();
        //return false;
    }
    doc.reset();
    return result;
}
bool ESPOTAGitHub::doUpgrade() {
    if (_upgradeURL.isEmpty()) {
		
		if(!checkUpgrade()) {
			return false;
		}
    } else {
		_setClock(); // Clock needs to be set to perform certificate checks
		// Don't need to do this if running check upgrade first, as it will have just been done there.
	}

	_resolveRedirects();
	
    urlDetails_t splitURL= _urlDetails(_upgradeURL);
	#if defined ESPOTAGitHub_DEBUG
        Serial.println( splitURL );
    #endif
    BearSSL::WiFiClientSecure client;
    _setClientSecure(client);

    bool mfln = client.probeMaxFragmentLength(splitURL.host, splitURL.port, 1024);
    if (mfln) {
        client.setBufferSizes(1024, 1024);
    }

    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.rebootOnUpdate(GHOTA_REBOOT_AFTER_UPGRADE);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, _upgradeURL);
    client.stop();

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            _lastError = ESPhttpUpdate.getLastErrorString();
            return false;

        case HTTP_UPDATE_NO_UPDATES:
            _lastError = F("HTTP_UPDATE_NO_UPDATES");
            return false;

        case HTTP_UPDATE_OK:
            _lastError = F("HTTP_UPDATE_OK");
            return true;
    }
    return false;
}

bool ESPOTAGitHub::doUpgrade(String& url) {
    _upgradeURL = url;
    return this->doUpgrade();
}

String ESPOTAGitHub::getLastError() {
    return _lastError;
}

String ESPOTAGitHub::getUpgradeURL() {
    return _upgradeURL;
}

String ESPOTAGitHub::getLatestTag() {
    return _releaseTag;
}
