
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

#define NVMRC_MAX_LENGTH 50
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
	bool has_error;
	bool is_installing;
	bool is_restoring;
	char* input_from_file;

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

	if (ctx.input_from_file) {
		#ifdef TRACE
		printf("TRACE: enter ctx.input_from_file branch\n");
		#endif
		free(ctx.input_from_file);
	}

	if (!ctx.has_error) {
		#ifdef TRACE
		printf("TRACE: enter not ctx.is_error branch\n");
		#endif
		return;
	}

	if (!ctx.paths.version) return;

	char* clean_cmd;
	bool format_error = asprintf(&clean_cmd, "rm -rf %s", ctx.paths.version) < 0;
	if (format_error) {
		perror("Failed to format cleanup command");
		return;
	}

	#ifdef TRACE
	printf("TRACE: clean_cmd %s\n", clean_cmd);
	#endif

	if (system(clean_cmd)) {
		printf("WARNING: Failed to clean up directory %s\n", ctx.paths.version);
		return;
	}
}

static size_t curl_on_data(void* ptr, size_t bytes, size_t nmemb, void* stream) {
	return fwrite(ptr, bytes, nmemb, (FILE*) stream);
}

static bool http_get_to_file(char* uri, FILE* f) {
	#ifdef TRACE
	printf("TRACE: enter http_get_to_file: %s\n", uri);
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
	bool success = (
		res != CURLE_HTTP_RETURNED_ERROR &&
		res == CURLE_OK
	);
	#ifdef TRACE
	if (!success) {
		printf("TRACE: got curl error %d: %s\n", res, curl_easy_strerror(res));
	}
	#endif

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	#ifdef TRACE
	printf("TRACE: enter http_get_to_file\n");
	#endif
	return success;
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
	printf("Usage:\n");
	printf("\tchnode use\n");
	printf("\tchnode <version>\n");
	printf("\tchnode < /some/version/file\n");
}

static bool show_current(void) {
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
			return false;
		}

		cmd_status = system(cmds[i]);
		if (cmd_status > 0) return false;
	}

	return true;
}

static bool parse_version(char* version) {
	#ifdef TRACE
	printf("TRACE: enter parse_version\n");
	#endif
	char* semver_parts[3];
	int i = 0;
	for (
		char* next_token = strtok(version, ".");
		next_token;
		next_token = strtok(NULL, ".")
	) {
		#ifdef TRACE
		printf("TRACE: idx %d: token %s\n", i, next_token);
		#endif
		semver_parts[i] = next_token;
		i += 1;
	}

	if (i != 3) {
		printf("Failed to parse version, exiting.\n");
		return false;

		// FIXME
		// support codenames
	}

	ctx.args.version = true;
	ctx.version.major = semver_parts[0];
	ctx.version.minor = semver_parts[1];
	ctx.version.patch = semver_parts[2];

	return true;
}

static bool read_fd(int fd) {
	#ifdef TRACE
	printf("TRACE: enter read_fd\n");
	#endif

	struct stat fd_stat;
	if (fstat(fd, &fd_stat) == -1) {
		perror("Failed to get length of file descriptor");
		return false;
	}

	if (!fd_stat.st_size) {
		#ifdef TRACE
		printf("TRACE: enter help branch\n");
		#endif
		ctx.args.help = true;
		return true;
	}

	if (fd_stat.st_size > NVMRC_MAX_LENGTH) {
		printf("Input byte length exceeds limit.\n");
		return false;
	}

	FILE* input = fdopen(fd, "rb");
	if (!input) {
		perror("Failed to open file descriptor for reading");
		return false;
	}

	ctx.input_from_file = malloc(fd_stat.st_size + 1);
	if (!ctx.input_from_file) {
		perror("Failed to allocate memory for input bytes");
		return false;
	}

	fread(ctx.input_from_file, 1, fd_stat.st_size, input);
	ctx.input_from_file[fd_stat.st_size] = 0;

	if (fclose(input) == EOF) {
		perror("Failed to close file descriptor");
		return false;
	}

	return parse_version(ctx.input_from_file);
}

static bool parse_nvmrc(void) {
	#ifdef TRACE
	printf("TRACE: enter parse_nvmrc\n");
	#endif

	char* cwd = getenv("PWD");
	if (!cwd) {
		printf("Failed to determine current working directory.\n");
		return false;
	}

	char* nvmrc_path;
	bool format_error = asprintf(&nvmrc_path, "%s/.nvmrc", cwd) < 0;
	if (format_error) {
		perror("Failed to construct path to .nvmrc");
		return false;
	}

	FILE* nvmrc_file = fopen(nvmrc_path, "rb");
	if (!nvmrc_file) {
		perror(nvmrc_path);
		return false;
	}

	int nvmrc_fd = fileno(nvmrc_file);
	return read_fd(nvmrc_fd);
}

static bool parse_stdin(void) {
	#ifdef TRACE
	printf("TRACE: enter parse_stdin\n");
	#endif

	return read_fd(STDIN_FILENO);
}

static bool chnode_dir(void) {
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
		return false;
	}

	bool mkdir_error = mkdir(ctx.paths.chnode, 0770) < 0;
	if (mkdir_error && errno != EEXIST) {
		perror("Failed to make chnode directory");
		return false;
	}

	return true;
}

static bool version_dir(void) {
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
		return false;
	}

	bool nodejs_path_error = access(ctx.paths.version, F_OK) < 0;
	bool not_exist = errno == ENOENT;

	if (not_exist) {
		bool mkdir_error = mkdir(ctx.paths.version, 0770) < 0;

		// in a race, this directory could already exist
		if (mkdir_error && errno != EEXIST) {
			perror("Failed to ensure version directory exists");
			return false;
		}
		ctx.is_installing = true;
		return true;
	}

	if (nodejs_path_error) {
		perror("Failed to ensure version directory exists");
		return false;
	}
	ctx.is_restoring = true;
	return true;
}

static bool release_dir(void) {
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
		return false;
	}

	bool nodejs_release_path_error = access(ctx.paths.release, F_OK) < 0;
	bool not_exist = errno == ENOENT;

	if (not_exist) {
		bool mkdir_error = mkdir(ctx.paths.release, 0770) < 0;
		if (mkdir_error && errno != EEXIST) {
			perror("Failed to ensure release directory exists");
			return false;
		}
		return true;
	}

	if (nodejs_release_path_error) {
		perror("Failed to ensure release directory exists");
		return false;
	}
	return true;
}

static bool download_and_verify(void) {
	#ifdef TRACE
	printf("TRACE: enter download_and_verify\n");
	#endif
	if (ctx.is_restoring) return true;

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
		return false;
	}

	nodejs_tarball = fopen(nodejs_download_path, "wb");
	if (!nodejs_tarball) {
		perror("Failed to open file path for download");
		return false;
	}

	downloaded = http_get_to_file(ctx.paths.tarball_uri, nodejs_tarball);
	if (fclose(nodejs_tarball)) {
		perror("Failed to close reference to tarball");
		return false;
	}
	if (!downloaded) {
		printf("Failed to download given version\n");
		return false;
	}

	#ifdef TRACE
	printf(
		"TRACE: major %s minor %s patch %s\n",
		ctx.version.major,
		ctx.version.minor,
		ctx.version.patch
	);
	#endif

	format_error = asprintf(
		&ctx.paths.shasums_uri,
		"%s/v%s.%s.%s/%s",
		NODEJS_DIST_BASE_URI,
		ctx.version.major,
		ctx.version.minor,
		ctx.version.patch,
		"SHASUMS256.txt"
	) < 0;
	if (format_error) {
		perror("Failed to construct URI to SHASUMS file");
		return false;
	}
	#ifdef TRACE
	printf("TRACE: shasums_uri %s\n", ctx.paths.shasums_uri);
	#endif

	format_error = asprintf(
		&ctx.paths.shasums,
		"%s/SHASUMS256.txt",
		ctx.paths.version
	) < 0;
	if (format_error) {
		perror("Failed to construct path for file download");
		return false;
	}

	nodejs_shasums = fopen(ctx.paths.shasums, "wb");
	if (!nodejs_shasums) {
		perror("Failed to open file path for download");
		return false;
	}

	downloaded = http_get_to_file(ctx.paths.shasums_uri, nodejs_shasums);
	if (fclose(nodejs_shasums)) {
		perror("Failed to close reference to SHASUMS file");
		return false;
	}
	if (!downloaded) {
		printf("Failed to download given SHASUMS file\n");
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
	if (format_error) {
		perror("Failed to format sha256sum command");
		return false;
	}

	cmd_status = system(shasum_command);
	if (cmd_status != 0) {
		printf("Failed to verify release signatures for given version. Exiting...\n");
		return false;
	}

	return true;
}

static bool extract_tarball(void) {
	#ifdef TRACE
	printf("TRACE: enter extract_tarball\n");
	#endif
	if (ctx.is_restoring) return true;

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
		return false;
	}

	cmd_status = system(tar_command);
	if (cmd_status == 127 || cmd_status == -1 || cmd_status != 0) {
		printf("Failed to extract tarball due to error %d. Exiting...\n", cmd_status);
		return false;
	}

	printf("Tarball extracted to %s\n", ctx.paths.release);
	return true;
}

static bool unlink_symlinks(void) {
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
			perror("Failed to format path to symlink");
			return false;
		}
		if (unlink(cmds[i]) == -1 && errno != ENOENT) {
			perror(cmds[i]);
			return false;
		}
	}

	return true;
}

static bool mk_symlinks(void) {
	#ifdef TRACE
	printf("TRACE: enter mk_symlinks\n");
	#endif

	// FIXME
	// parameterise this process so the compiler can unroll it (hopefully)

	bool format_error, symlink_error;

	format_error = asprintf(&ctx.paths.node_src, "%s/bin/node", ctx.paths.release) < 0;
	if (format_error) {
		perror("Failed to format path to symlink");
		return false;
	}

	format_error = asprintf(&ctx.paths.node_dst, "%s/bin/node", PREFIX) < 0;
	if (format_error) {
		perror("Failed to format path to symlink");
		return false;
	}

	format_error = asprintf(&ctx.paths.npm_src, "%s/bin/npm", ctx.paths.release) < 0;
	if (format_error) {
		perror("Failed to format path to symlink");
		return false;
	}

	format_error = asprintf(&ctx.paths.npm_dst, "%s/bin/npm", PREFIX) < 0;
	if (format_error) {
		perror("Failed to format path to symlink");
		return false;
	}

	format_error = asprintf(&ctx.paths.npx_src, "%s/bin/npx", ctx.paths.release) < 0;
	if (format_error) {
		perror("Failed to format path to symlink");
		return false;
	}

	format_error = asprintf(&ctx.paths.npx_dst, "%s/bin/npx", PREFIX) < 0;
	if (format_error) {
		perror("Failed to format path to symlink");
		return false;
	}

	symlink_error = symlink(ctx.paths.node_src, ctx.paths.node_dst) < 0;
	if (symlink_error) {
		perror(ctx.paths.node_dst);
		return false;
	}

	symlink_error = symlink(ctx.paths.npm_src, ctx.paths.npm_dst) < 0;
	if (symlink_error) {
		perror(ctx.paths.npm_dst);
		return false;
	}

	symlink_error = symlink(ctx.paths.npx_src, ctx.paths.npx_dst) < 0;
	if (symlink_error) {
		perror(ctx.paths.npx_dst);
		return false;
	}

	return true;
}

static bool test_binaries(void) {
	#ifdef TRACE
	printf("TRACE: enter test_binaries\n");
	#endif
	char* test_node;
	char* test_npm;
	bool format_error;

	format_error = asprintf(&test_node, "%s -v", ctx.paths.node_dst) < 0;
	if (format_error) {
		perror("Failed to format binary test command");
		return false;
	}

	format_error = asprintf(&test_npm, "%s -v", ctx.paths.npm_dst) < 0;
	if (format_error) {
		perror("Failed to format binary test command");
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

	if (atexit(trap_exit)) {
		perror("Failed to register exit handler. Exiting...\n");
		return false;
	}

	if (argc > 1) {

		// is the first arg a call for help?
		if (
			!strncmp(argv[1], "-h", 2) ||
			!strncmp(argv[1], "help", 4)
		) {
			ctx.args.help = true;
			return true;
		}

		// or a call to use?
		if (!strncmp(argv[1], "use", 3)) {
			ctx.args.use = true;
			return parse_nvmrc();
		}

		// or a version?
		ctx.args.version = parse_version(argv[1]);
		return ctx.args.version;
	}

	// otherwise, try and read a version from stdin
	ctx.args.version = true;
	return parse_stdin();
}

int chnode(int argc, char** argv) {
	#ifdef TRACE
	printf("TRACE: enter dispatch_command\n");
	#endif

	if (!parse_arguments(argc, argv)) {
		#ifdef TRACE
		printf("TRACE: enter enter parse_arguments fail branch\n");
		#endif
		return EXIT_FAILURE;
	}

	if (ctx.args.help) {
		#ifdef TRACE
		printf("TRACE: enter help branch\n");
		#endif
		show_intro();
		show_usage();
		show_current();
		return EXIT_SUCCESS;
	}

	if (ctx.args.version) {
		#ifdef TRACE
		printf("TRACE: enter version branch\n");
		#endif
		printf(
			"Using v%s.%s.%s...\n",
			ctx.version.major,
			ctx.version.minor,
			ctx.version.patch
		);

		if (
			chnode_dir() &&
			version_dir() &&
			release_dir() &&
			download_and_verify() &&
			extract_tarball() &&
			unlink_symlinks() &&
			mk_symlinks() &&
			test_binaries()
		) return EXIT_SUCCESS;

		ctx.has_error = true;
	}

	#ifdef TRACE
	printf("TRACE: enter failure branch\n");
	#endif

	return EXIT_FAILURE;
}
