
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

#ifndef PREFIX
#define PREFIX "/usr/local"
#warning "chnode is being installed with the default global prefix: /usr/local"
#endif

#define NODEJS_DIST_BASE_URI "https://nodejs.org/dist"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

static struct chnode {
	bool is_error;
	bool is_recoverable;
	bool installing;
	bool restoring;

	struct args {
		bool help;
		bool current;
		bool use;
		bool version;
	} args;

	struct version {
		char* major;
		char* minor;
		char* patch;
	} version;

	struct paths {
		char* node_src;
		char* npm_src;
		char* npx_src;

		char* node_dst;
		char* npm_dst;
		char* npx_dst;

		char* home;
		char* prefix;
		char* chnode;

		char* version;
		char* release;
		char* tarball;
		char* shasums;

		char* tarball_uri;
		char* shasums_uri;
	} paths;

} ctx;

static void trap_exit() {
	#ifdef TRACE
	printf("TRACE: enter on_exit\n");
	#endif

	if (!ctx.is_error) return;
	if (!ctx.is_recoverable) {
		printf("WARNING: Detected an unrecoverable error\n");
		printf("WARNING: Consider nuking your chnode directory before retrying\n");
		return;
	}

	char* clean_cmd;
	bool format_error = asprintf(&clean_cmd, "rm -rf %s", ctx.paths.version) < 0;
	if (format_error) {
		printf("WARNING: Failed to clean up .chnode directory\n");
		return;
	}

	if (system(clean_cmd)) {
		printf("WARNING: Failed to clean up %s\n", ctx.paths.version);
		return;
	}
}

static size_t curl_on_data(void* ptr, size_t bytes, size_t nmemb, void* stream) {
	return fwrite(ptr, bytes, nmemb, (FILE*) stream);
}

static bool http_get_to_file(char* uri, FILE* f) {
	#ifdef TRACE
	printf("TRACE: enter http_get_to_file\n");
	#endif
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

static void show_intro(void) {
	#ifdef TRACE
	printf("TRACE: enter show_intro\n");
	#endif
	printf("chnode :: Install and use different versions of Node.js\n");
	printf("version 0.0.1\n");
	printf("Copyright (c) 2020 Ant Cosentino\n");
	printf("https://git.sr.ht/~ac/chnode\n");
}

static void show_usage(void) {
	#ifdef TRACE
	printf("TRACE: enter show_usage\n");
	#endif
	printf("Usage: chnode <version>\n");
}

static bool show_current() {
	#ifdef TRACE
	printf("TRACE: enter show_current\n");
	#endif
	bool format_error;
	int cmd_status;
	char* cmds[3];
	char* binaries[3] = {
		"bin/node",
		"bin/npm",
		"bin/npx"
	};

	for (size_t i = 0; i < 3; i += 1) {
		format_error = asprintf(
			&cmds[i],
			"ls -la %s/%s",
			PREFIX,
			binaries[i]
		) < 0;
		if (format_error) {
			perror("Failed to format command");
			ctx.is_error = true;
			return false;
		}

		cmd_status = system(cmds[i]);
		if (cmd_status > 0) {
			ctx.is_error = true;
			return false;
		}
	}

	return true;
}

static bool parse_version(char* version) {
	#ifdef TRACE
	printf("TRACE: enter parse_version\n");
	#endif
	size_t v_ptr = 0;
	char* versions[3];
	char* next_token = strtok(version, ".");
	if (!next_token) return false;
	versions[v_ptr] = next_token;
	while ((next_token = strtok(NULL, "."))) {
		v_ptr += 1;
		if (v_ptr > 2) return false;
		versions[v_ptr] = next_token;
	}
	if (v_ptr != 2) return false;

	ctx.version.major = versions[0];
	ctx.version.minor = versions[1];
	ctx.version.patch = versions[2];

	// FIXME
	// the version can be a.b.c

	return true;
}

static bool chnode_dir() {
	#ifdef TRACE
	printf("TRACE: enter chnode_dir\n");
	#endif
	ctx.paths.home = getenv("HOME");
	bool format_error = asprintf(
		&ctx.paths.chnode,
		"%s/.chnode",
		ctx.paths.home
	) < 0;
	if (format_error) {
		perror("Failed to construct path to chnode directory");
		ctx.is_error = true;
		ctx.is_recoverable = false;
		return false;
	}

	bool mkdir_error = mkdir(ctx.paths.chnode, 0770) < 0;
	if (mkdir_error && errno != EEXIST) {
		perror("Failed to make chnode directory");
		ctx.is_error = true;
		ctx.is_recoverable = false;
		return false;
	}

	return true;
}

static bool version_dir() {
	#ifdef TRACE
	printf("TRACE: enter version_dir\n");
	#endif
	bool format_error = asprintf(
		&ctx.paths.version,
		"%s/%s.%s.%s",
		ctx.paths.chnode,
		ctx.version.major,
		ctx.version.minor,
		ctx.version.patch
	) < 0;

	if (format_error) {
		perror("Failed to construct path to directory for given version");
		ctx.is_error = true;
		ctx.is_recoverable = false;
		return false;
	}

	bool nodejs_path_error = access(ctx.paths.version, F_OK) < 0;
	bool not_exist = errno == ENOENT;

	if (not_exist) {
		bool mkdir_error = mkdir(ctx.paths.version, 0770) < 0;

		// in a race, this directory could already exist
		if (mkdir_error && errno != EEXIST) {
			perror("Failed to ensure version directory exists");
			ctx.is_error = true;
			ctx.is_recoverable = false;
			return false;
		}
		ctx.installing = true;
		return true;
	}

	if (nodejs_path_error) {
		perror("Failed to ensure version directory exists");
		ctx.is_error = true;
		ctx.is_recoverable = false;
		return false;
	}
	ctx.restoring = true;
	return true;
}

static bool release_dir() {
	#ifdef TRACE
	printf("TRACE: enter release_dir\n");
	#endif
	bool format_error = asprintf(
		&ctx.paths.release,
		"%s/release",
		ctx.paths.version
	) < 0;
	if (format_error) {
		perror("Failed to construct path to directory for given version");
		ctx.is_error = true;
		ctx.is_recoverable = false;
		return false;
	}

	bool nodejs_release_path_error = access(ctx.paths.release, F_OK) < 0;
	bool not_exist = errno == ENOENT;

	if (not_exist) {
		bool mkdir_error = mkdir(ctx.paths.release, 0770) < 0;
		if (mkdir_error && errno != EEXIST) {
			perror("Failed to ensure release directory exists");
			ctx.is_error = true;
			ctx.is_recoverable = false;
			return false;
		}
		return true;
	}

	if (nodejs_release_path_error) {
		perror("Failed to ensure release directory exists");
		ctx.is_error = true;
		ctx.is_recoverable = false;
		return false;
	}
	return true;
}

static bool download_and_verify() {
	#ifdef TRACE
	printf("TRACE: enter download_and_verify\n");
	#endif
	if (ctx.restoring) return true;

	bool format_error, downloaded;
	int cmd_status;
	FILE *nodejs_tarball, *nodejs_shasums;

	format_error = asprintf(
		&ctx.paths.tarball,
		"node-v%s.%s.%s-%s-x64.tar.gz",
		ctx.version.major,
		ctx.version.minor,
		ctx.version.patch,
		UNAME
	) < 0;

	if (format_error) {
		perror("Failed to construct tarball file path");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(
		&ctx.paths.tarball_uri,
		"%s/v%s.%s.%s/%s",
		NODEJS_DIST_BASE_URI,
		ctx.version.major,
		ctx.version.minor,
		ctx.version.patch,
		ctx.paths.tarball
	) < 0;

	if (format_error) {
		perror("Failed to construct URI for given version");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	char* nodejs_download_path;
	format_error = asprintf(
		&nodejs_download_path,
		"%s/%s",
		ctx.paths.version,
		ctx.paths.tarball
	) < 0;

	if (format_error) {
		perror("Failed to construct path for file download");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	nodejs_tarball = fopen(nodejs_download_path, "wb");
	if (!nodejs_tarball) {
		perror("Failed to open file path for download");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	downloaded = http_get_to_file(ctx.paths.tarball_uri, nodejs_tarball);
	if (fclose(nodejs_tarball)) {
		perror("Failed to close reference to tarball");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}
	if (!downloaded) {
		printf("Failed to download given version\n");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(
		&ctx.paths.shasums_uri,
		"%s/v%s.%s.%s/SHASUMS256.txt",
		NODEJS_DIST_BASE_URI,
		ctx.version.major,
		ctx.version.minor,
		ctx.version.patch
	) < 0;
	if (format_error) {
		perror("Failed to construct URI to SHASUMS file");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(
		&ctx.paths.shasums,
		"%s/SHASUMS256.txt",
		ctx.paths.version
	) < 0;
	if (format_error) {
		perror("Failed to construct path for file download");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	nodejs_shasums = fopen(ctx.paths.shasums, "wb");
	if (!nodejs_shasums) {
		perror("Failed to open file path for download");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	downloaded = http_get_to_file(ctx.paths.shasums_uri, nodejs_shasums);
	if (fclose(nodejs_shasums)) {
		perror("Failed to close reference to SHASUMS file");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}
	if (!downloaded) {
		printf("Failed to download given SHASUMS file\n");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	char* shasum_command;
	format_error = asprintf(
		&shasum_command,
		"cd %s && grep %s %s | sha256sum -c -",
		ctx.paths.version,
		ctx.paths.tarball,
		ctx.paths.shasums
	) < 0;

	cmd_status = system(shasum_command);
	if (cmd_status != 0) {
		printf("Failed to verify release signatures for given version. Exiting...\n");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	return true;
}

static bool extract_tarball() {
	#ifdef TRACE
	printf("TRACE: enter extract_tarball\n");
	#endif
	if (ctx.restoring) return true;

	char* tar_command;
	bool format_error;
	int cmd_status;

	format_error = asprintf(
		&tar_command,
		"tar -C %s/release --strip-components=1 -xf %s/%s",
		ctx.paths.version,
		ctx.paths.version,
		ctx.paths.tarball
	) < 0;
	if (format_error) {
		perror("Failed to construct command for extracting tarball");
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	cmd_status = system(tar_command);
	if (cmd_status == 127 || cmd_status == -1 || cmd_status != 0) {
		printf("Failed to extract tarball due to error %d. Exiting...\n", cmd_status);
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	printf("Tarball extracted to %s\n", ctx.paths.release);
	return true;
}

static bool unlink_symlinks() {
	#ifdef TRACE
	printf("TRACE: enter unlink_symlinks\n");
	#endif
	bool format_error;
	char* cmds[3];
	char* binaries[3] = {
		"node",
		"npm",
		"npx"
	};

	for (size_t i = 0; i < 3; i += 1) {
		format_error = asprintf(&cmds[i], "%s/bin/%s", PREFIX, binaries[i]) < 0;
		if (format_error) {
			ctx.is_error = true;
			ctx.is_recoverable = true;
			return false;
		}
		if (unlink(cmds[i]) == -1 && errno != ENOENT) {
			perror(cmds[i]);
			ctx.is_error = true;
			ctx.is_recoverable = true;
			return false;
		}
	}

	return true;
}

static bool mk_symlinks() {
	#ifdef TRACE
	printf("TRACE: enter mk_symlinks\n");
	#endif

	bool format_error, symlink_error;

	format_error = asprintf(&ctx.paths.node_src, "%s/bin/node", ctx.paths.release) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(&ctx.paths.node_dst, "%s/bin/node", PREFIX) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(&ctx.paths.npm_src, "%s/bin/npm", ctx.paths.release) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(&ctx.paths.npm_dst, "%s/bin/npm", PREFIX) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(&ctx.paths.npx_src, "%s/bin/npx", ctx.paths.release) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(&ctx.paths.npx_dst, "%s/bin/npx", PREFIX) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	symlink_error = symlink(ctx.paths.node_src, ctx.paths.node_dst) < 0;
	if (symlink_error) {
		perror(ctx.paths.node_dst);
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	symlink_error = symlink(ctx.paths.npm_src, ctx.paths.npm_dst) < 0;
	if (symlink_error) {
		perror(ctx.paths.npm_dst);
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	symlink_error = symlink(ctx.paths.npx_src, ctx.paths.npx_dst) < 0;
	if (symlink_error) {
		perror(ctx.paths.npx_dst);
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	return true;
}

static bool test_binaries() {
	#ifdef TRACE
	printf("TRACE: enter test_binaries\n");
	#endif
	char* test_node;
	char* test_npm;
	bool format_error;

	format_error = asprintf(&test_node, "%s -v", ctx.paths.node_dst) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	format_error = asprintf(&test_npm, "%s -v", ctx.paths.npm_dst) < 0;
	if (format_error) {
		ctx.is_error = true;
		ctx.is_recoverable = true;
		return false;
	}

	int node_status = system(test_node);
	int npm_status = system(test_npm);

	return !(node_status && npm_status);
}

static bool parse_arguments(int argc, char** argv) {
	#ifdef TRACE
	printf("TRACE: enter parse_arguments\n");
	#endif
	// TODO
	// accept a command via stdin
	if (
		argc < 2 ||
		!strncmp(argv[1], "-h", 2) ||
		!strncmp(argv[1], "help", 4)
	) {
		ctx.args.help = true;
		return true;
	}

	if (atexit(on_exit)) {
		perror("Failed to register exit handler. Exiting...\n");
		return false;
	}

	if (!strncmp(argv[1], "use", 3)) {
		ctx.args.use = true;
		return true;
	}

	ctx.args.version = parse_version(argv[1]);
	return ctx.args.version;
}

int dispatch_command(int argc, char** argv) {
	#ifdef TRACE
	printf("TRACE: enter dispatch_command\n");
	#endif

	if (!parse_arguments(argc, argv)) return EXIT_FAILURE;
	if (!chnode_dir()) return EXIT_FAILURE;

	// TODO
	// dispatch with one variable and function pointers?
	if (ctx.args.help) {
		show_intro();
		show_usage();
		show_current();
		return EXIT_SUCCESS;
	}

	if (ctx.args.use) {
		// TODO
		// from stdin/argv
		return EXIT_SUCCESS;
	}

	if (ctx.args.version) {
		printf(
			"Using v%s.%s.%s...\n",
			ctx.version.major,
			ctx.version.minor,
			ctx.version.patch
		);

		if (
			version_dir() &&
			release_dir() &&
			download_and_verify() &&
			extract_tarball() &&
			unlink_symlinks() &&
			mk_symlinks() &&
			test_binaries()
		) return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
