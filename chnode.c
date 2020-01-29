
#ifdef __linux__
#define UNAME "linux"
#elif defined __APPLE__
#define UNAME "darwin"
#elif defined __WIN64__
#define UNAME "win"
#else
#error "chnode does not support this operating system"
#endif

#define NODEJS_DIST_BASE_URI "https://nodejs.org/dist/"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

// static size_t write_callback(void* ptr, size_t bytes, size_t nmemb, void* stream) {
// 	return fwrite(ptr, bytes, nmemb, (FILE*) stream);
// }

// void http_get(char* uri, char* file_path) {

// }


// TODO
// * name this fn better
// * try and improve its ergonomics
size_t use(char* version, char* out[3]) {
	size_t version_ptr = 0;
	char* next_token = strtok(version, ".");
	if (!next_token) return 0;
	out[version_ptr] = next_token;
	while ((next_token = strtok(NULL, "."))) {
		version_ptr += 1;
		if (version_ptr > 2) return 0;
		out[version_ptr] = next_token;
	}
	if (version_ptr != 2) return 0;
	return 1;
}

int main(int argc, char** argv) {
	printf("chnode version 0.0.1\n");
	printf("Copyright (c) 2020 Ant Cosentino\n");
	printf("https://git.sr.ht/~ac/chnode\n\n");

	// was this invoked with no arguments or
	// one positional arg equivalent to -h?
	if (argc < 2 || !strncmp(argv[1], "-h", 2)) {
		printf("Usage: chnode <version>\n");
		return EXIT_FAILURE;
	}

	char* version[3];
	size_t parsed = use(argv[1], version);
	if (!parsed) {
		printf("Failed to parse version number. Exiting...\n");
		return EXIT_FAILURE;
	}

	printf("Using v%s.%s.%s\n", version[0], version[1], version[2]);

	// the file path is likely yo be at most 22 characters long
	char nodejs_path[22];
	int path_format_result = snprintf(
		nodejs_path,
		sizeof nodejs_path,
		"~/.chnode/%s.%s.%s",
		version[0],
		version[1],
		version[2]
	);

	if (path_format_result < 0) {
		printf(
			"Failed to construct path to directory for v%s.%s.%s. Exiting...\n",
			version[0],
			version[1],
			version[2]
		);
		return EXIT_FAILURE;
	}

	printf(
		"Checking for existing installation of v%s.%s.%s\n",
		version[0],
		version[1],
		version[2]
	);

	if (access(nodejs_path, F_OK) == -1) {
		printf(
			"v%s.%s.%s not found. Downloading...\n",
			version[0],
			version[1],
			version[2]
		);

		// TODO
		// download this release to ~/.chnode

		// the uri is likely to be at most 75 characters long
		char uri[75];
		int uri_format_result = snprintf(
			uri,
			sizeof uri,
			// https://nodejs.org/dist/latest-v12.x/node-v12.14.1-linux-arm64.tar.gz
			"%slatest-v%s.x/node-v%s.%s.%s-%s-x64.tar.gz",
			NODEJS_DIST_BASE_URI,
			version[0],
			version[0],
			version[1],
			version[2],
			UNAME
		);

		if (uri_format_result < 0) {
			printf(
				"Failed to construct URI for version %s.%s.%s and uname %s. Exiting...\n",
				version[0],
				version[1],
				version[2],
				UNAME
			);
			return EXIT_FAILURE;
		}

		printf("OK\n");
	}

	// TODO
	// set the symlinks up to point to the release

	return EXIT_SUCCESS;

	// // curl_global_init(CURL_GLOBAL_DEFAULT);
	// // CURL* curl = curl_easy_init();

	// // if (!curl) {
	// // 	curl_global_cleanup();
	// // 	printf("Failed to initialise libcurl. Exiting...\n");
	// // 	return EXIT_FAILURE;
	// // }

	// // curl_easy_setopt(curl, CURLOPT_URL, uri);

	// // CURLcode res = curl_easy_perform(curl);

	// // // TODO
	// // // store the tarball in a directory managed by chnode (maybe it should
	// // // be a dot-dir owned by the current user?)


	// return EXIT_SUCCESS;
}
