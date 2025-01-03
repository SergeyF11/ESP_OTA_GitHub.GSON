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
#if defined GITHUB_CERTIFICATE_ROOT and defined GITHUB_CERTIFICATE_ROOT1     
    if ( _cert->getCount() == 0 ){      
        _cert->append(GITHUB_CERTIFICATE_ROOT);
        _cert->append(GITHUB_CERTIFICATE_ROOT1);
        OTAdebugPrintf("Imported %d certs\n", _cert->getCount() );
    }
#else
        #warning ("Need append certificate after init")
#endif
    
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
    String path;
    //int port = 80;
    if (url.startsWith( _http )){ //"http://")) {
        urlDetailP->proto = _http; //"http://";
        urlDetailP->port = 80;
        //url.replace( _http /*"http://"*/, "");
        path = url.substring(7);
    } else {
        urlDetailP->proto = _https; //"https://";
        urlDetailP->port = 443;
        //url.replace(_https /*"https://"*/, "");
        path = url.substring(8);
    }

    //OTAdebugPrintf("path=%s\n", path.c_str());
    int firstSlash = path.indexOf('/');
    urlDetailP->host = path.substring(0, firstSlash);;
    urlDetailP->path = path.substring(firstSlash);;
    OTAdebugPrintln( *urlDetailP );
    return *urlDetailP;
};


bool ESPOTAGitHub::_setClientSecure(BearSSL::WiFiClientSecure& c){
OTAdebugPrintf("Set client [%x] use ", &c);

    if ( _certStore != nullptr ){
        c.setCertStore(_certStore);    
        OTAdebugPrintf("certificate store [%x]\n", _certStore );
        return true;

    } else if ( _cert != nullptr ){
        c.setTrustAnchors( _cert );
        OTAdebugPrintf("X509 list [%x]", _cert->getCount() );
        return true;
    } else {
        c.setInsecure();
        OTAdebugPrintln("insecure connection");
        
    }
    return false;
}

bool ESPOTAGitHub::_resolveRedirects() {
    OTAdebugPretty;

    urlDetails_t splitURL = _urlDetails(_upgradeURL);
    String proto = splitURL.proto;
    String host = splitURL.host;
    int port = splitURL.port;
    String path = splitURL.path;
    bool isFinalURL = false;

    static BearSSL::WiFiClientSecure client;
    _setClientSecure(client);

    // if ( _certStore == nullptr ){
    //     client.setInsecure();
    // } else {
    //     client.setCertStore(_certStore);
    // }

    while (!isFinalURL) {
        //if ( client.connected() ) client.stop();
        if ( ! client.connect(host, port) ) {
            _lastError = F("Connection Failed.");
            OTAdebugPrintln( _lastError );
            return false;
        } else {
            OTAdebugPrintf("Client [%x] connected to %s\n", &client, host.c_str());
        }

    ESPOTAGitHub::_HTTPget(client, host.c_str(), path.c_str(), nullptr );
    // client.print(F("GET "));
    // client.print(path);
    // client.print(F(" HTTP/1.1\r\n"));
    // client.print(F("Host: ") ); client.print( host );
    // client.print(F("\r\n"));
    // client.print(F("User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n"));
    // client.print(F("Connection: close\r\n\r\n"));

        // client.print(String("GET ") + path + " HTTP/1.1\r\n" +
        //     "Host: " + host + "\r\n" +
        //     "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n" +
        //     "Connection: close\r\n\r\n");
        urlDetails_t url;
        // String response;
        // response.reserve(2000);
        
        while (client.connected()) {
            String response = client.readStringUntil('\n');
            //su::Text response(  client.readStringUntil('\n') );
            
            if ( response.substring(0,9).equalsIgnoreCase( F("location:")) ){
            //if (response.startsWith("location: ") || response.startsWith("Location: ")) {
                isFinalURL = false;

                // String location;
                // location.reserve(2000);
                // location = response.substring(10, response.length() -1 );
                // response = NULL_STR;
                // response.reserve(0);
                response = response.substring(10, response.length() -1 );
                OTAdebugPretty;
                OTAdebugPrint("Location: ");
                OTAdebugPrintln( response );

                if (response.startsWith(_http) || response.startsWith(_https)) {
                    OTAdebugPrintln("//absolute URL - separate host from path");
                    
                    url = _urlDetails(response);
                    proto = url.proto;
                    host = url.host;
                    port = url.port;
                    path = url.path;
                } else {
					//relative URL - host is the same as before, location represents the new path.
                    path = response;
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
    if( client.connected()) client.stop();
    
    if(isFinalURL) {
    OTAdebugPrint("Final URL found: ");
        // String finalURL = proto + host + path;
        // _upgradeURL = finalURL;
        _upgradeURL = String( proto );
        _upgradeURL += host;
        _upgradeURL += path;
    OTAdebugPrintln( _upgradeURL );

        return true;
    } else {
        _lastError = F("NO_FINAL_URL_FOUND");
        return false;
    }
}

// Set time via NTP, as required for x.509 validation

void ESPOTAGitHub::_setClock() {
    if ( time(nullptr) > _likeRealTime ) return;
    OTAdebugPrintln("Time config. Start sync time...");

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
    OTAdebugPrintln( (timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec) );
}

/* Public methods */

bool ESPOTAGitHub::checkUpgrade() {
    OTAdebugPretty;

    _setClock(); // Clock needs to be set to perform certificate checks
    BearSSL::WiFiClientSecure client;

        OTAdebugPrintf("Set secure for new client [%x]\n", &client );
    _setClientSecure(client);

    if (!client.connect(GHOTA_HOST, GHOTA_PORT)) {
        _lastError = "Connection failed";
        return false;
    }

    ESPOTAGitHub::_HTTPget(client, GHOTA_HOST, "repos", _user, _repo, "releases/latest", nullptr );

    // client.print(F("GET /repos/"));

    // client.print( _user );
    // client.print('/');
    // client.print( _repo);
    // client.print(F("/releases/latest"));
  
    // client.print(F(" HTTP/1.1\r\n"));
    // client.print(F("Host: ") ); client.print( GHOTA_HOST );
    // client.print(F("\r\n"));
    // client.print(F("User-Agent: ESP8266\r\n"));
    // client.print(F("Connection: close\r\n\r\n"));
    
    client.setTimeout(GHOTA_TIMEOUT);

    //OTArunStart;
    while( client.connected() ) {
        String response = client.readStringUntil('\n');
        if ( response == "\r" ) {
			break;
        }
    }
    //OTAprintRunTime;

    //OTArunStart;    
    String response = client.readString(); //Until('\n');
    //OTAprintRunTime;

    gson::Parser doc;
    // while ( response.startsWith("{") && ! doc.parse(response.c_str(), response.length())){
    //     delay(10);
    //     Serial.println(response);
    //     response += client.readString();
    // }
    //if ( client.connected()){ client.stop(); }
    bool result = false;
        OTAdebugPretty;
        OTAdebugPrintln( response.c_str() );

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
                    releaseNote = doc["name"].toString();
                        
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
    //doc.reset();
    return result;
}
bool ESPOTAGitHub::doUpgrade() {
OTAdebugPretty;
    if (_upgradeURL.isEmpty()) {
		
		if(!checkUpgrade()) {
			return false;
		}
    } else {
		_setClock(); // Clock needs to be set to perform certificate checks
		// Don't need to do this if running check upgrade first, as it will have just been done there.
	}

	if ( _resolveRedirects() ){
	
    urlDetails_t splitURL= _urlDetails(_upgradeURL);


OTAdebugPrint("Redirect to ");
OTAdebugPrintln( splitURL );

    BearSSL::WiFiClientSecure client;
    _setClientSecure(client);

OTAdebugPrintln("Check mfln for upgrade");

    bool mfln = client.probeMaxFragmentLength(splitURL.host, splitURL.port, 1024);
    if (mfln) {
        client.setBufferSizes(1024, 1024);
    }

    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.rebootOnUpdate(GHOTA_REBOOT_AFTER_UPGRADE);

OTAdebugPrintln("Start esp update");

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, _upgradeURL);
    //client.stop();

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
    //

    // } else {
    //     _lastError = F("NO_RIDERECT");
        
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
