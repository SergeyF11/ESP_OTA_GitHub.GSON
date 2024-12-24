/*
  ESP_OTA_GitHub.cpp - ESP library for auto updating code from GitHub releases.
  Created by Gavin Smalley, November 13th 2019.
  Released under the LGPL v2.1.
  It is the author's intention that this work may be used freely for private
  and commercial use so long as any changes/improvements are freely shared with
  the community under the same terms.
*/

#include "ESP_OTA_GitHub.h"

ESPOTAGitHub::ESPOTAGitHub(BearSSL::CertStore* certStore, const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease) {
    _certStore = certStore;
    _user = user;
    _repo = repo;
    _currentTag = currentTag;
    _binFile = binFile;
    _preRelease = preRelease;
    _lastError = NULL_STR;
    _upgradeURL = NULL_STR;
    _releaseTag = NULL_STR;
}
ESPOTAGitHub::ESPOTAGitHub( const char* user, const char* repo, const char* currentTag, const char* binFile, bool preRelease) {
    _certStore = nullptr;
    _user = user;
    _repo = repo;
    _currentTag = currentTag;
    _binFile = binFile;
    _preRelease = preRelease;
    _lastError = NULL_STR;
    _upgradeURL = NULL_STR;
    _releaseTag = NULL_STR;
}

static const char _http[] PROGMEM = "http://";
static const char _https[] PROGMEM = "https://";

/* Private methods */
urlDetails_t ESPOTAGitHub::_urlDetails(String url) {
    String proto = "";
    int port = 80;
    if (url.startsWith( _http )){ //"http://")) {
        proto = _http; //"http://";
        url.replace( _http /*"http://"*/, "");
    } else {
        proto = _https; //"https://";
        port = 443;
        url.replace(_https /*"https://"*/, "");
    }
    int firstSlash = url.indexOf('/');
    String host = url.substring(0, firstSlash);
    String path = url.substring(firstSlash);

    urlDetails_t urlDetail;

    urlDetail.proto = proto;
    urlDetail.host = host;
    urlDetail.port = port;
    urlDetail.path = path;

    return urlDetail;
};

urlDetails_t ESPOTAGitHub::_urlDetails(urlDetails_t& urlDetail, String url) {
    //String proto = "";
    //int port = 80;
    if (url.startsWith( _http )){ //"http://")) {
        urlDetail.proto = _http; //"http://";
        urlDetail.port = 80;
        url.replace( _http /*"http://"*/, "");
    } else {
        urlDetail.proto = _https; //"https://";
        urlDetail.port = 443;
        url.replace(_https /*"https://"*/, "");
    }
    int firstSlash = url.indexOf('/');
    // String host = url.substring(0, firstSlash);
    // String path = url.substring(firstSlash);

    //urlDetail.proto = proto;
    urlDetail.host = url.substring(0, firstSlash);
    //urlDetail.port = port;
    urlDetail.path = url.substring(firstSlash);;

    return urlDetail;
};

bool ESPOTAGitHub::_resolveRedirects() {
    urlDetails_t splitURL = _urlDetails(_upgradeURL);
    String proto = splitURL.proto;
    String host = splitURL.host;
    int port = splitURL.port;
    String path = splitURL.path;
    bool isFinalURL = false;

    BearSSL::WiFiClientSecure client;
    if ( _certStore == nullptr ){
        client.setInsecure();
    } else {
        client.setCertStore(_certStore);
    }

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
            // Serial.print("Resp:");
            // Serial.println( response);
            // Serial.println( response.substring(0,9));

            if ( response.substring(0,9).equalsIgnoreCase( F("location:")) ){
            //if (response.startsWith("location: ") || response.startsWith("Location: ")) {
                isFinalURL = false;
                String location = response.substring(10, response.length() -1 );
                
				// if (response.startsWith("location: ")) {
				// 	location.replace("location: ", "");
				// } else {
				// 	location.replace("Location: ", "");
				// }
                //location.remove(location.length() - 1);
                //Serial.printf("Location: %s\n", location.c_str());

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

    if(isFinalURL) {
        String finalURL = proto + host + path;
        _upgradeURL = finalURL;
        //Serial.printf("Final URL=%s\n", finalURL.c_str() );
        
        return true;
    } else {
        _lastError = F("CONNECTION FAILED");
        return false;
    }
}

// Set time via NTP, as required for x.509 validation
void ESPOTAGitHub::_setClock() {
    if ( _certStore == nullptr || time(nullptr) > 8 *3600 *2 ) return;
    //Serial.println(__PRETTY_FUNCTION__);
	configTime("GMT-0", GHOTA_NTP1, GHOTA_NTP2);  // UTC

	time_t now = time(nullptr);
	while (now < 8 * 3600 * 2) {
		yield();
        Serial.print(".");
		delay(500);
		now = time(nullptr);
	}
    //Serial.println(" done");
	struct tm timeinfo;
	gmtime_r(&now, &timeinfo);
}

/* Public methods */

bool ESPOTAGitHub::checkUpgrade() {
	
    BearSSL::WiFiClientSecure client;
    if ( _certStore == nullptr ){
        client.setInsecure();
//        Serial.print("Insecure git client");
    }else {
        _setClock(); // Clock needs to be set to perform certificate checks
        client.setCertStore(_certStore);
//        Serial.print("Secure git client");
    }
    if (!client.connect(GHOTA_HOST, GHOTA_PORT)) {
        _lastError = "Connection failed";
        return false;
    }
	//Serial.print("Connect "GHOTA_HOST":"); Serial.println(GHOTA_PORT);  
    // String url = "/repos/";
    // url += _user;
    // url += "/";
    // url += _repo;
    // url += "/releases/latest";

    //Serial.println("Send GET request");  
    
    // client.print(String("GET ") + url + " HTTP/1.1\r\n" +
    //     "Host: " + GHOTA_HOST + "\r\n" +
    //     "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n" +
    //     "Connection: close\r\n\r\n");
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
    
/*     Serial.println( String("GET ") + url + " HTTP/1.1\r\n" +
        "Host: " + GHOTA_HOST + "\r\n" +
        "User-Agent: ESP_OTA_GitHubArduinoLibrary\r\n" +
        "Connection: close\r\n\r\n"); */
//    Serial.println("sended. Wait response...");

    while (client.connected()) {
        String response = client.readStringUntil('\n');
        if (response == "\r") {
			break;
        }
    }


    client.setTimeout(GHOTA_TIMEOUT);
    String response = client.readString();
    gson::Parser doc;
    while ( client.available() || ! doc.parse(response.c_str(), response.length())){
        delay(10);
        Serial.println(response);
        response += client.readString();
    }

		if ( doc.has("tag_name")) {
            //Serial.print("tag name found: ");
			_releaseTag = doc["tag_name"].toString();
            String release_name = doc["name"].toString();
            bool release_prerelease = doc["prerelease"].toBool();
            //bool release_prerelease = doc["Pre_Beta_release"];
//Serial.println(_releaseTag );
           
			if (strcmp(_releaseTag.c_str(), _currentTag) != 0) {
				if (!_preRelease) {
					if (release_prerelease) {
						_lastError = F("Latest release is a pre-release and GHOTA_ACCEPT_PRERELEASE is set to false.");
						return false;
					}
				}

                if ( doc.has("assets") && doc["assets"].isArray() ){ //&& doc["assets"].isArray() ){
// Serial.print("Assets:");  doc["assets"].stringify(Serial); Serial.println();
                int i = 0;
                bool valid_asset = false;
                
                while(  doc["assets"][i].isObject() ){
                
                //Serial.printf("Asset[%d]:%s\n", i, doc["assets"][i]["name"].toString().c_str()); 
                //doc["assets"][i].stringify(Serial);
                //Serial.println();

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
                doc.reset();
				if (valid_asset) {
                    //Serial.println("Parsing done");
					return true;
				} else {
					_lastError = F("No valid binary found for latest release.");
					return false;
				}
			} else {
				_lastError = F("Already running latest release.");
                return false;
			}
		} else {
			_lastError = F("JSON didn't match expected structure. 'tag_name' missing.");
            return false;
		}
	} else {
		_lastError = F("Failed to parse JSON."); // Error was: " + error.c_str();
        return false;
	}
}
bool ESPOTAGitHub::doUpgrade() {
    if (_upgradeURL.isEmpty()) {
        //_lastError = "No upgrade URL set, run checkUpgrade() first.";
        //return false;
		
		if(!checkUpgrade()) {
			return false;
		}
    } else {
		_setClock(); // Clock needs to be set to perform certificate checks
		// Don't need to do this if running check upgrade first, as it will have just been done there.
	}

	_resolveRedirects();
	
    urlDetails_t splitURL= _urlDetails(_upgradeURL);
	
    BearSSL::WiFiClientSecure client;
    bool mfln = client.probeMaxFragmentLength(splitURL.host, splitURL.port, 1024);
    if (mfln) {
        client.setBufferSizes(1024, 1024);
    }
    if ( _certStore == nullptr ){
        client.setInsecure();
    }else {
        client.setCertStore(_certStore);
    }
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.rebootOnUpdate(false);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, _upgradeURL);

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
