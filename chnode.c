
#ifdef __linux__
#define UNAME "linux"
#elif defined __APPLE__
#define UNAME "darwin"
#elif defined __WIN64__
#include <windows.h>
#define mkdir(dir, mode) _mkdir(dir)
#define UNAME "win"
#else
#error "chnode does not support this operating system"
#endif

#define NODEJS_DIST_BASE_URI "https://nodejs.org/dist"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <curl/curl.h>

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

// TODO
// delete directories created for failed downloads (ie. http 404)
int main(int argc, char** argv) {

	// was this invoked with no arguments or -h?
	if (argc < 2 || !strncmp(argv[1], "-h", 2)) {
		printf("chnode :: Install and use different versions of Node.js\n");
		printf("version 0.0.1\n");
		printf("Copyright (c) 2020 Ant Cosentino\n");
		printf("https://git.sr.ht/~ac/chnode\n");
		printf("Usage: chnode <version>\n");
		return EXIT_FAILURE;
	}

	char* version[3];
	size_t parsed = use(argv[1], version);
	if (!parsed) {
		printf("Failed to parse version number. Exiting...\n");
		return EXIT_FAILURE;
	}

	printf("Using v%s.%s.%s...\n", version[0], version[1], version[2]);

	bool format_error, mkdir_error, symlink_error;

	const char* HOME = getenv("HOME");

	char* chnode_path;
	format_error = asprintf(&chnode_path, "%s/.chnode", HOME) < 0;

	if (format_error) {
		perror("Failed to construct path to chnode directory");
		free(chnode_path);
		return EXIT_FAILURE;
	}

	mkdir_error = mkdir(chnode_path, 0770) < 0;
	if (mkdir_error && errno != EEXIST) {
		perror("Failed to make chnode directory");
		free(chnode_path);
		return EXIT_FAILURE;
	}

	char* nodejs_path;
	format_error = asprintf(
		&nodejs_path,
		"%s/%s.%s.%s",
		chnode_path,
		version[0],
		version[1],
		version[2]
	) < 0;

	if (format_error) {
		perror("Failed to construct path to directory for given version");
		free(chnode_path);
		return EXIT_FAILURE;
	}

	char* nodejs_release_path;
	format_error = asprintf(&nodejs_release_path, "%s/release", nodejs_path) < 0;
	if (format_error) {
		perror("Failed to construct path to directory for given version");
		free(chnode_path);
		free(nodejs_path);
		return EXIT_FAILURE;
	}

	bool nodejs_path_exists = !access(nodejs_path, F_OK);
	if (!nodejs_path_exists) {

		mkdir_error = mkdir(nodejs_path, 0770) < 0;
		if (mkdir_error) {
			perror("Failed to make release parent directory");
			free(chnode_path);
			return EXIT_FAILURE;
		}

		mkdir_error = mkdir(nodejs_release_path, 0770) < 0;
		if (mkdir_error) {
			perror("Failed to make release directory");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_release_path);
			return EXIT_FAILURE;
		}

		char* nodejs_tarball_path;
		format_error = asprintf(
			&nodejs_tarball_path,
			"node-v%s.%s.%s-%s-x64.tar.gz",
			version[0],
			version[1],
			version[2],
			UNAME
		) < 0;

		if (format_error) {
			perror("Failed to construct tarball file path");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			return EXIT_FAILURE;
		}

		char* nodejs_uri;
		format_error = asprintf(
			&nodejs_uri,
			// https://nodejs.org/dist/latest-v12.x/node-v12.14.1-linux-arm64.tar.gz
			"%s/latest-v%s.x/%s",
			NODEJS_DIST_BASE_URI,
			version[0],
			nodejs_tarball_path
		) < 0;

		if (format_error) {
			perror("Failed to construct URI for given version");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			return EXIT_FAILURE;
		}

		char* nodejs_download_path;
		format_error = asprintf(
			&nodejs_download_path,
			"%s/%s",
			nodejs_path,
			nodejs_tarball_path
		) < 0;

		if (format_error) {
			perror("Failed to construct path for file download");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}

		FILE* nodejs_tarball;
		nodejs_tarball = fopen(nodejs_download_path, "wb");
		if (!nodejs_tarball) {
			perror("Failed to open file path for download");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}

		bool downloaded = http_get_to_file(nodejs_uri, nodejs_tarball);
		if (!downloaded) {
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}

		// TODO
		// check the return code
		fclose(nodejs_tarball);

		char* tar_command;
		format_error = asprintf(
			&tar_command,
			"tar -C %s/release --strip-components=1 -xf %s/%s",
			nodejs_path,
			nodejs_path,
			nodejs_tarball_path
		) < 0;

		if (format_error) {
			perror("Failed to construct command for extracting tarball");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}

		int code = system(tar_command);
		if (code == 127 || code == -1 || code != 0) {
			printf("Failed to extract tarball due to error %d. Exiting...\n", code);
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			free(tar_command);
			return EXIT_FAILURE;
		} else {
			printf("Tarball extracted to %s\n", nodejs_release_path);
		}

		free(nodejs_tarball_path);
		free(nodejs_uri);
		free(nodejs_download_path);
		free(tar_command);

		// TODO
		// verify the release signatures
	}

	bool unlink_node_error = (
		unlink("/usr/local/bin/node") == -1 &&
		errno != ENOENT
	);
	if (unlink_node_error) {
		perror("Failed to unlink node symlink");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		return EXIT_FAILURE;
	}

	bool unlink_npm_error = (
		unlink("/usr/local/bin/npm") == -1 &&
		errno != ENOENT
	);
	if (unlink_npm_error) {
		perror("Failed to unlink npm symlink");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		return EXIT_FAILURE;
	}

	bool unlink_npx_error = (
		unlink("/usr/local/bin/npx") == -1 &&
		errno != ENOENT
	);
	if (unlink_npx_error) {
		perror("Failed to unlink npm symlink");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		return EXIT_FAILURE;
	}

	printf("Creating symbolic links...\n");

	char* node;
	format_error = asprintf(&node, "%s/bin/node", nodejs_release_path) < 0;
	if (format_error) {
		perror("Failed to construct path to binary");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		return EXIT_FAILURE;
	}

	symlink_error = symlink(node, "/usr/local/bin/node") < 0;
	if (symlink_error) {
		perror("Failed to symlink node");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		free(node);
		return EXIT_FAILURE;
	}

	char* npm;
	format_error = asprintf(&npm, "%s/bin/npm", nodejs_release_path) < 0;
	if (format_error) {
		perror("Failed to construct path to binary");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		free(node);
		return EXIT_FAILURE;
	}

	symlink_error = symlink(npm, "/usr/local/bin/npm") < 0;
	if (symlink_error) {
		perror("Failed to symlink npm");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		free(node);
		free(npm);
		return EXIT_FAILURE;
	}

	char* npx;
	format_error = asprintf(&npx, "%s/bin/npx", nodejs_release_path) < 0;
	if (format_error) {
		perror("Failed to construct path to binary");
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		free(node);
		free(npm);
		return EXIT_FAILURE;
	}
	symlink_error = symlink(npx, "/usr/local/bin/npx") < 0;
	if (symlink_error) {
		free(chnode_path);
		free(nodejs_path);
		free(nodejs_release_path);
		free(node);
		free(npm);
		free(npx);
		return EXIT_FAILURE;
	}

	printf("OK\n");

	free(chnode_path);
	free(nodejs_path);
	free(nodejs_release_path);
	free(node);
	free(npm);
	free(npx);

	int node_version = system("/usr/local/bin/node -v");
	int npm_version = system("/usr/local/bin/npm -v");

	return (
		node_version || npm_version ?
		EXIT_FAILURE :
		EXIT_SUCCESS
	);
}
