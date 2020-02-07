
#include <curl/curl.h>

static const char* version[];

void on_exit() {
	const char* home = getenv("HOME");
	char* clean_cmd;

	// TODO share the version here somehow
	bool format_error = asprintf(
		&clean_cmd,
		"rm -rf %s/.chnode/%s.%s.%s",
		home,
		version[0],
		version[1],
		version[2]
	) < 0;

	int status = system(clean_cmd);
	if (status != 0) {
		printf(
			"Failed to clean up %s/.chnode/%s.%s.%s",
			home,
			version[0],
			version[1],
			version[2]
		);
	}
}

size_t curl_on_data(void* ptr, size_t bytes, size_t nmemb, void* stream) {
	return fwrite(ptr, bytes, nmemb, (FILE*) stream);
}

int http_get_to_file(char* uri, FILE* f) {
	CURL* curl;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, uri);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_on_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return (
		res != CURLE_HTTP_RETURNED_ERROR &&
		res == CURLE_OK
	);
}

bool parse_version(char* version, char* out[3]) {
	size_t version_ptr = 0;
	char* next_token = strtok(version, ".");
	if (!next_token) return true;
	out[version_ptr] = next_token;
	while ((next_token = strtok(NULL, "."))) {
		version_ptr += 1;
		if (version_ptr > 2) return true;
		out[version_ptr] = next_token;
	}
	if (version_ptr != 2) return true;
	return false;
}