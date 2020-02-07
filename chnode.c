
#ifdef __linux__
#define _GNU_SOURCE
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
	bool error = parse_version(argv[1], version);
	if (error) {
		printf("Failed to parse version number. Exiting...\n");
		return EXIT_FAILURE;
	}

	printf("Using v%s.%s.%s...\n", version[0], version[1], version[2]);

	int cmd_status;
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
			"%s/v%s.%s.%s/%s",
			NODEJS_DIST_BASE_URI,
			version[0],
			version[1],
			version[2],
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

		bool downloaded;

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

		downloaded = http_get_to_file(nodejs_uri, nodejs_tarball);
		if (fclose(nodejs_tarball)) {
			perror("Failed to close reference to tarball");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}
		if (!downloaded) {
			printf("Failed to download given version\n");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}

		char* nodejs_shasums_uri;
		format_error = asprintf(
			&nodejs_uri,
			"%s/v%s.%s.%s/SHASUMS256.txt",
			NODEJS_DIST_BASE_URI,
			version[0],
			version[1],
			version[2]
		) < 0;

		if (format_error) {
			perror("Failed to construct URI to SHASUMS file");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}

		FILE* nodejs_shasums;
		nodejs_shasums = fopen(nodejs_download_path, "wb");
		if (!nodejs_shasums) {
			perror("Failed to open file path for download");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			return EXIT_FAILURE;
		}

		downloaded = http_get_to_file(nodejs_shasums_uri, nodejs_shasums);
		if (fclose(nodejs_shasums)) {
			perror("Failed to close reference to SHASUMS file");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			free(nodejs_shasums_uri);
			return EXIT_FAILURE;
		}
		if (!downloaded) {
			printf("Failed to download given SHASUMS file\n");
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			free(nodejs_shasums_uri);
			return EXIT_FAILURE;
		}

		char* shasum_command;
		format_error = asprintf(
			&shasum_command,
			"sha256sum -c <(grep %s %s/SHASUMS256.txt)",
			nodejs_tarball_path,
			nodejs_path
		) < 0;

		cmd_status = system(shasum_command);
		if (cmd_status != 0) {
			printf("Failed to verify release signatures for given version. Exiting...\n", );
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			free(nodejs_shasums_uri);
			free(shasum_command);
			return EXIT_FAILURE;
		}

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
			free(nodejs_shasums_uri);
			return EXIT_FAILURE;
		}

		cmd_status = system(tar_command);
		if (cmd_status == 127 || cmd_status == -1 || cmd_status != 0) {
			printf("Failed to extract tarball due to error %d. Exiting...\n", cmd_status);
			free(chnode_path);
			free(nodejs_path);
			free(nodejs_tarball_path);
			free(nodejs_uri);
			free(nodejs_download_path);
			free(nodejs_shasums_uri);
			free(tar_command);
			return EXIT_FAILURE;
		}
		printf("Tarball extracted to %s\n", nodejs_release_path);

		free(nodejs_tarball_path);
		free(nodejs_uri);
		free(nodejs_download_path);
		free(nodejs_shasums_uri);
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
