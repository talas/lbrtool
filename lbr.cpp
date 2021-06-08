/*
   LBR Tool -- Build and extract from C64 LBR archives

   Copyright 2020 Talas (talas.pw)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string>
#include <iostream>
#include <filesystem> // c++17
#include <fstream>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <getopt.h>

const int MAX_SANE_LENGTH = 1024*1024;
static bool verbose = false;
static bool convert_petscii = true;

static struct option long_options[] = {
	{"sort",          no_argument, NULL, 'n'},
	{"pad-sorted",    no_argument, NULL, 'p'},
	{"strip",         no_argument, NULL, 's'},
	{"skip-deleted",  no_argument, NULL, 'b'},
	{"add-extension", no_argument, NULL, 'X'},
	{"no-conversion", no_argument, NULL, 'P'},
	{"append",        no_argument, NULL, 'a'},
	{"list",          no_argument, NULL, 'l'},
	{"create",        no_argument, NULL, 'c'},
	{"extract",       no_argument, NULL, 'e'},
	{"extract-into", required_argument, NULL, 'E'},
	{"delete",       required_argument, NULL, 'd'},
	{"wipe",         required_argument, NULL, 'w'},
	{"type",         required_argument, NULL, 't'},
	{"help",       no_argument, NULL, 'h'},
	{"version",    no_argument, NULL, 'V'},
	{"verbose",    no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

struct FileEntry {
	std::string name;
	std::string path;
	unsigned int length;
	std::string type;
	bool bad_length;
};

static std::string petscii2ascii(std::string petscii) {
	// Note: this is a very conservative conversion
	if (!convert_petscii) return petscii;
	std::string ascii;
	for (const char & c : petscii) {
		if (c < 0x20) // ' '
			ascii += '?';
		else if (c >= 0x61 && c <= 0x7A)
			ascii += c - 0x20;
		else if (c >= 0xC1 && c <= 0xCA)
			ascii += c - 0x80;
		else if (c == 0x5B || c == 0x5D)
			ascii += c;
		else if(c > 0x5A) { // 'Z'
			ascii += '?';
		} else
			ascii += c;
	}
	return ascii;
}

static std::string ascii2petscii(std::string ascii) {
	// Note: this is a very conservative conversion
	if (!convert_petscii) return ascii;
	std::string petscii;
	for (const char & c : ascii) {
		if (c < 0x20) // ' '
			petscii += '?';
		else if (c == 0x5C) // backslash
			petscii += '/';
		else if (c == 0x5F) // under_score
			petscii += ' ';
		else if (c == 0x60) // backtick
			petscii += 0x27; // apostrophe
		else if (c >= 0x61 && c <= 0x7A) // lower case
			petscii += c - 0x20;
		else if (c == 0x7B) // curly open
			petscii += '(';
		else if (c == 0x7D) // curly close
			petscii += ')';
		else if (c == 0x7C) // pipe
			petscii += '/';
		else if (c > 0x7C)
			petscii += '?';
		else
			petscii += c;
	}
	return petscii;
}

static bool num_cmp(const FileEntry & i,const FileEntry & j) {
	if (i.name.length() < j.name.length())
		return true;
	if (i.name.length() > j.name.length())
		return false;
	return i.name.compare(j.name) < 0;
}

int build_lbr(std::string outfile, std::vector<std::string> input,
	bool numerical_sort, bool numerical_padding, bool strip_extension) {

	std::vector<FileEntry> files;
	for (const auto & i : input) {
		std::filesystem::path path(i);
		FileEntry f;
		f.name = path.filename();
		f.path = i;
		f.length = std::filesystem::file_size(path);
		files.push_back(f);
	}
	if (numerical_sort) {
		std::sort(files.begin(), files.end(), num_cmp);
		if (numerical_padding) {
			std::string f_str = files.front().name;
			int first = std::atoi(f_str.c_str());
			std::string l_str = files.back().name;
			int last = std::atoi(l_str.c_str());
			if (last <= first) {
				std::cout << "Error. unable to pad.";
				return 1;
			}
			for(int i = first; i < last; i++) {
				std::string c_str = files[i-first].name;
				int cur = std::atoi(c_str.c_str());
				while(i < cur) {
					FileEntry f;
					f.name = std::to_string(i);
					f.length = 0;
					files.insert(files.begin()+(i-first),f);
					++i;
				}
			}
		}
	}
	std::ofstream out(outfile, std::ofstream::binary);
	out << "DWB" << " ";
	out << files.size() << " ";
	out.put(0x0D);
	for (const auto & a : files) {
		if (verbose)
			std::cout << "+ " << a.name;
		std::string ext = std::filesystem::path(a.name).extension();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](auto c){ return std::tolower(c); });
		if (strip_extension) {
			size_t dot = a.name.find_last_of(".");
			if (dot == std::string::npos)
				out << ascii2petscii(a.name);
			else
				out << ascii2petscii(a.name.substr(0, dot));
		} else
			out << ascii2petscii(a.name);
		out.put(0x0D);
		if (a.length == 0)
			out.put('D');
		else {
			if (ext == ".prg")
				out.put('P');
			else if (ext == ".usr")
				out.put('U');
			else if (ext == ".rel")
				out.put('R');
			else
				out.put('S');
		}
		out.put(0x0D);
		out << " " << a.length << " ";
		out.put(0x0D);
	}
	for (const auto & f : files) {
		if (f.length == 0) continue;
		std::ifstream infile(f.path, std::ifstream::binary);
		char* buffer = new char[f.length];
		infile.read(buffer, f.length);
		out.write(buffer, f.length);
		delete[] buffer;
	}
	out.close();
	return 0;
}

int extract_lbr(std::string infile, std::string dest_folder, std::vector<std::string> targets, bool skip_deleted, bool add_extension) {
	std::ifstream in(infile, std::ifstream::binary);
	char signature[3];
	in.read(signature, 3);
	if (signature[0] != 'D' || signature[1] != 'W' || signature[2] != 'B') {
		in.close();
		std::cout << "Error: invalid signature, not an LBR file?" << std::endl;
		return 1;
	}
	in.ignore(1); // space
	char count[256];
	in.get(count, 256, 0x20);
	in.ignore(1); // space
	in.ignore(1); // cr
	std::vector<FileEntry> files;
	int file_count = atoi(count);
	for (int i = 0; i < file_count; ++i) {
		// TODO: error handling
		char name[256];
		in.get(name, 256, 0x0D);
		in.ignore(1);
		char type[256];
		in.get(type, 256, 0x0D);
		in.ignore(1); // cr
		in.ignore(1); // space
		char length[256];
		in.get(length, 256, 0x20);
		in.ignore(1); // space
		in.ignore(1); // cr
		FileEntry f;
		f.name = name;
		int len = atoi(length);
		if (len < 0 || len > MAX_SANE_LENGTH) {
			f.bad_length = true;
			std::cout << "Found file with bad length" << std::endl;
		}
		f.length = len;
		f.type = type;
		files.push_back(f);
	}
	for (const auto & f : files) {
		if (f.bad_length) {
			// Just glob up everything
			std::filesystem::path fp = dest_folder;
			fp /= f.name;
			std::ofstream out(fp, std::ofstream::binary);
			auto pos = in.tellg();
			in.seekg(0, in.end);
			int length = in.tellg() - pos;
			in.seekg(pos, in.beg);
			char* buffer = new char[length];
			in.read(buffer, length);
			out.write(buffer, length);
			out.close();
			delete[] buffer;
			break;
		}
		if (f.length == 0) continue;
		if (f.type == "D" && skip_deleted) {
			in.ignore(f.length);
			continue;
		}
		if (!targets.empty()) {
			bool found = false;
			for (const auto & t : targets) {
				if (petscii2ascii(f.name) == t) {
					found = true;
					break;
				}
			}
			if(!found) in.ignore(f.length);
		}
		std::filesystem::path fp = dest_folder;
		std::string ascii_name = petscii2ascii(f.name);
		if (add_extension) {
			if (f.type == "P")
				ascii_name += ".prg";
			else if (f.type == "S")
				ascii_name += ".seq";
			else if (f.type == "U")
				ascii_name += ".usr";
			else if (f.type == "R")
				ascii_name += ".rel";
		}
		fp /= ascii_name;
		std::ofstream out(fp, std::ofstream::binary);
		char* buffer = new char[f.length];
		in.read(buffer, f.length);
		out.write(buffer, f.length);
		out.close();
		delete[] buffer;
	}

	in.close();
	return 0;
}

int find_in_lbr(std::string infile, std::string target, int & length, long & offset, long & diroffset, bool skip_deleted) {
	std::ifstream in(infile, std::ifstream::binary);
	char signature[3];
	in.read(signature, 3);
	if (signature[0] != 'D' || signature[1] != 'W' || signature[2] != 'B') {
		in.close();
		std::cout << "Error: invalid signature, not an LBR file?" << std::endl;
		return -1;
	}
	in.ignore(1); // space
	char count[256];
	in.get(count, 256, 0x20);
	in.ignore(1); // space
	in.ignore(1); // cr
	int file_count = atoi(count);
	long coff = 0;
	bool found = false;
	for (int i = 0; i < file_count; ++i) {
		// TODO: error handling
		long doff = in.tellg();
		char name[256];
		in.get(name, 256, 0x0D);
		in.ignore(1);
		char type[256];
		in.get(type, 256, 0x0D);
		in.ignore(1); // cr
		in.ignore(1); // space
		char len[256];
		in.get(len, 256, 0x20);
		in.ignore(1); // space
		in.ignore(1); // cr
		int l = atoi(len);
		if (!found && petscii2ascii(name) == target) {
			if (type[0] != 'D' || !skip_deleted) {
				length = l;
				diroffset = doff;
				found = true;
			}
		}
		if (!found)
			coff += l;
		if (l < 0 || l > MAX_SANE_LENGTH) {
			std::cout << "Found file with bad length" << std::endl;
			return -1;
		}
	}
	if (found) {
		offset = in.tellg() + coff;
		return 0;
	}
	return -1;
}

int delete_lbr(std::string file, std::string target, bool skip_deleted, bool wipe) {
	int length = 0;
	long offset = 0;
	long dir_offset = 0;
	int res = find_in_lbr(file, target, length, offset, dir_offset, skip_deleted);
	if (res != 0) {
		std::cout << "No deletion occured." << std::endl;
		return res;
	}
	std::FILE* tmpf = std::tmpfile();
	std::ifstream in(file, std::ifstream::binary);

	char signature[4];
	in.read(signature, 3);
	signature[3] = 0x00;
	std::fputs(signature, tmpf);
	if (wipe) {
		std::fputc(in.get(), tmpf); // space
		char count[256];
		in.get(count, 256, 0x20);
		int file_count = atoi(count);
		std::fputs(std::to_string(file_count-1).c_str(), tmpf);
		std::fputc(in.get(), tmpf); // space
		std::fputc(in.get(), tmpf); // cr
	}

	char c;
	while (in.get(c) && in.tellg() < dir_offset)
		std::fputc(c, tmpf);
	std::fputc(c, tmpf);
	if (wipe) {
		while (in.get(c) && c != 0x0D) {} // name
		while (in.get(c) && c != 0x0D) {} // type
		while (in.get(c) && c != 0x0D) {} // size
	} else {
		// Update directory entry.
		while (in.get(c) && c != 0x0D)
			std::fputc(c, tmpf);
		std::fputc(0x0D, tmpf);
		std::fputc('D', tmpf);
		std::fputc(0x0D, tmpf);
		std::fputs(" 0 ", tmpf);
		std::fputc(0x0D, tmpf);
		while (in.get(c) && c != 0x0D) {} // type
		while (in.get(c) && c != 0x0D) {} // size
	}

	while (in.get(c) && in.tellg() <= offset)
		std::fputc(c, tmpf);
	in.seekg(offset + length, in.beg); // skip over the file
	while (in.get(c))
		std::fputc(c, tmpf);
	if (!in.eof()) {
		std::cout << "Error reading from archive." << std::endl;
	}
	in.close();
	std::ofstream out(file, std::ofstream::binary);
	std::rewind(tmpf);
	int ch;
	while ((ch = std::fgetc(tmpf)) != EOF) {
		out.put(ch);
	}
	out.close();
	std::fclose(tmpf);
	return 0;
}

int chtype_lbr(std::string file, std::string target, std::string new_type, bool skip_deleted) {
	int length = 0;
	long offset = 0;
	long dir_offset = 0;
	int res = find_in_lbr(file, target, length, offset, dir_offset, skip_deleted);
	if (res != 0) {
		std::cout << "Failed" << std::endl;
		return res;
	}
	std::FILE* tmpf = std::tmpfile();
	std::ifstream in(file, std::ifstream::binary);

	char signature[4];
	in.read(signature, 3);
	signature[3] = 0x00;
	std::fputs(signature, tmpf);

	char c;
	while (in.get(c) && in.tellg() < dir_offset)
		std::fputc(c, tmpf);
	std::fputc(c, tmpf);
	while (in.get(c) && c != 0x0D)
		std::fputc(c, tmpf); // name
	std::fputc(0x0D, tmpf);
	std::fputs(ascii2petscii(new_type).c_str(), tmpf);
	std::fputc(0x0D, tmpf);
	while (in.get(c) && c != 0x0D) {} // type
	while (in.get(c))
		std::fputc(c, tmpf);
	if (!in.eof()) {
		std::cout << "Error reading from archive." << std::endl;
	}
	in.close();
	std::ofstream out(file, std::ofstream::binary);
	std::rewind(tmpf);
	int ch;
	while ((ch = std::fgetc(tmpf)) != EOF) {
		out.put(ch);
	}
	out.close();
	std::fclose(tmpf);
	return 0;
}

int add_lbr(std::string file, std::vector<std::string> targets, bool strip_extension) {
	long offset = 0;
	std::FILE* tmpf = std::tmpfile();
	std::ifstream in(file, std::ifstream::binary);
	char signature[4];
	in.read(signature, 3);
	if (signature[0] != 'D' || signature[1] != 'W' || signature[2] != 'B') {
		in.close();
		std::cout << "Error: invalid signature, not an LBR file?" << std::endl;
		return -1;
	}
	signature[3] = 0x00;
	std::fputs(signature, tmpf);
	std::fputc(in.get(), tmpf); // space
	char count[256];
	in.get(count, 256, 0x20);
	int file_count = atoi(count);
	std::fputs(std::to_string(file_count+targets.size()).c_str(), tmpf);
	std::fputc(in.get(), tmpf); // space
	std::fputc(in.get(), tmpf); // cr
	std::vector<FileEntry> files;
	for (const auto & a : targets) {
		FileEntry f;
		f.name = std::filesystem::path(a).filename();
		f.path = a;
		f.length = std::filesystem::file_size(a);
		files.push_back(f);
	}

	for (int i = 0; i < file_count; ++i) {
		// TODO: error handling
		char c;
		while (in.get(c) && c != 0x0D)
			std::fputc(c, tmpf); // name
		std::fputc(0x0D, tmpf);
		while (in.get(c) && c != 0x0D)
			std::fputc(c, tmpf); // type
		std::fputc(0x0D, tmpf);
		std::fputc(in.get(), tmpf); // space
		char len[256];
		in.get(len, 256, 0x20);
		std::fputs(len, tmpf);
		std::fputc(in.get(), tmpf); // space
		std::fputc(in.get(), tmpf); // cr
		int l = atoi(len);
		offset += l;
		if (l < 0 || l > MAX_SANE_LENGTH) {
			std::cout << "Found file with bad length" << std::endl;
			return -1;
		}
	}
	offset += in.tellg();
	for (const auto & f : files) {
		if (verbose)
			std::cout << "+ " << f.name;
		std::string ext = std::filesystem::path(f.name).extension();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](auto c){ return std::tolower(c); });
		if (strip_extension) {
			size_t dot = f.name.find_last_of(".");
			if (dot == std::string::npos)
				std::fputs(ascii2petscii(f.name).c_str(), tmpf);
			else
				std::fputs(ascii2petscii(f.name.substr(0, dot)).c_str(), tmpf);
		} else
			std::fputs(ascii2petscii(f.name).c_str(), tmpf);
		std::fputc(0x0D, tmpf);
		if (f.length == 0)
			std::fputc('D', tmpf);
		else {
			if (ext == ".prg")
				std::fputc('P', tmpf);
			else if (ext == ".usr")
				std::fputc('U', tmpf);
			else if (ext == ".rel")
				std::fputc('R', tmpf);
			else
				std::fputc('S', tmpf);
		}
		std::fputc(0x0D, tmpf);
		std::fputc(0x20, tmpf);
		std::fputs(std::to_string(f.length).c_str(), tmpf);
		std::fputc(0x20, tmpf);
		std::fputc(0x0D, tmpf);
	}

	char c;
	while (in.get(c) && in.tellg() <= offset)
		std::fputc(c, tmpf);

	for (const auto & f : files) {
		if (f.length > 0) {
			std::ifstream infile(f.path, std::ifstream::binary);
			char* buffer = new char[f.length];
			infile.read(buffer, f.length);
			std::fwrite(buffer, sizeof buffer[0], f.length, tmpf);
			delete[] buffer;
		}
	}

	while (in.get(c))
		std::fputc(c, tmpf);
	if (!in.eof()) {
		std::cout << "Error reading from archive." << std::endl;
	}
	in.close();
	std::ofstream out(file, std::ofstream::binary);
	std::rewind(tmpf);
	int ch;
	while ((ch = std::fgetc(tmpf)) != EOF) {
		out.put(ch);
	}
	out.close();
	std::fclose(tmpf);
	return 0;
}

int list_lbr(std::string file, bool skip_deleted, bool sort_numerical) {
	std::ifstream in(file, std::ifstream::binary);
	char signature[4];
	in.read(signature, 3);
	if (signature[0] != 'D' || signature[1] != 'W' || signature[2] != 'B') {
		in.close();
		std::cout << "Error: invalid signature, not an LBR file?" << std::endl;
		return -1;
	}
	in.ignore(1); // space
	char count[256];
	in.get(count, 256, 0x20);
	in.ignore(1); // space
	in.ignore(1); // cr
	int file_count = atoi(count);
	std::string basename = std::filesystem::path(file).filename();
	if (verbose)
		std::cout << basename << " " << file_count << " entries" << std::endl;

	std::vector<FileEntry> files;
	for (int i = 0; i < file_count; ++i) {
		// TODO: error handling
		char name[256];
		in.get(name, 256, 0x0D);
		in.ignore(1);
		char type[256];
		in.get(type, 256, 0x0D);
		in.ignore(1); // cr
		in.ignore(1); // space
		char length[256];
		in.get(length, 256, 0x20);
		in.ignore(1); // space
		in.ignore(1); // cr
		FileEntry f;
		f.name = name;
		f.length = atoi(length);
		f.type = type;
		files.push_back(f);
	}
	if (sort_numerical)
		std::sort(files.begin(), files.end(), num_cmp);
	for (const auto & f : files) {
		if (f.type == "D" && skip_deleted) {
			if (verbose)
				std::cout << "[deleted]" << std::endl;
			continue;
		} else
			std::cout << petscii2ascii(f.name) << " (" << petscii2ascii(f.type) << ") " << f.length;
		if (f.length < 0 || f.length > MAX_SANE_LENGTH) {
			if (verbose)
				std::cout << " (bad)";
		}
		std::cout << std::endl;
	}
	in.close();
	return 0;
}

enum class Op {
	Nothing,
	List,
	Create,
	Extract,
	Append,
	Delete,
	Wipe,
	ChangeType
};

static void print_help()
{
	printf("\
Usage: lbr ACTION [OPTIONS] ARCHIVE [FILES...]\n");
	fputs("\
Create, extract and modify C64 LBR archives.\n", stdout);
	puts("");
	fputs("\
  -h, --help          display this help and exit\n\
  -V, --version       display version information and exit\n\
  -v, --verbose       increase verbosity of printing\n", stdout);
	puts("");
	fputs("\
 Actions:\n\
  -a, --append               add files to the end of the archive\n\
  -l, --list                 print out entries in the archive (default action)\n\
  -c, --create               create an archive with the given files\n\
  -e, --extract              extract from the archive\n\
  -E, --extract-into=FOLDER  extract from the archive, into the given FOLDER\n\
  -t, --type=FILENAME:TYPE   change filetype of a file in the archive to TYPE\n\
  -d, --delete=FILENAME      delete a file from the archive, keeping the entry\n\
  -w, --wipe=FILENAME        delete a file from the archive completely\n", stdout);
	puts("");
	fputs("\
 Options for actions:\n\
  -n, --sort            when creating archive and printing entries, sort files numerically\n\
  -b, --skip-deleted    skip over files marked as deleted (filetype D)\n\
  -s, --strip           remove extensions when adding files to archive\n\
  -X, --add-extension   adds an extension to extracted files\n\
  -p, --pad-sorted      when creating sorted archive, add deleted files as padding (Advanced)\n\
  -P, --no-conversion   do not convert between ASCII and PETSCII (Advanced)\n", stdout);
	puts("");
	fputs("\
Please backup files before using the program.\n", stdout);
	printf ("\n");
}
// TODO: strip should be default on

static void print_version();

int main(int argc, char *argv[])
{
	int optc;
	std::string path = std::filesystem::current_path();
	std::string target_file;
	bool sort_numerical = false;
	bool sort_numerical_padding = false;
	bool strip_extensions = false;
	bool skip_deleted = false;
	bool add_extension = false;
	Op operation = Op::List;
	int opcount = 0;
	std::string new_type;

	while ((optc = getopt_long(argc, argv, "ad:lceE:w:t:npsbXPhvV", long_options, NULL)) != -1)
		switch (optc) {
			case 'h':
				print_help();
				exit(0);
			case 'V':
				print_version();
				exit(0);
			case 'v':
				verbose = true;
				break;
			case 'a':
				opcount += 1;
				operation = Op::Append;
				break;
			case 'l':
				opcount += 1;
				operation = Op::List;
				break;
			case 'c':
				opcount += 1;
				operation = Op::Create;
				break;
			case 'd':
				opcount += 1;
				operation = Op::Delete;
				if (!optarg) abort();
				target_file = optarg;
				break;
			case 'w':
				opcount += 1;
				operation = Op::Wipe;
				if (!optarg) abort();
				target_file = optarg;
				break;
			case 't':
			{
				opcount += 1;
				operation = Op::ChangeType;
				if (!optarg) abort();
				std::string str(optarg);
				size_t cln = str.find_last_of(":");
				if (cln == std::string::npos) {
					std::cout << "Missing separator in argument." << std::endl;
					print_help();
					exit(1);
				}
				target_file = str.substr(0, cln);
				new_type = str.substr(cln+1);
				break;
			}
			case 'e':
				opcount += 1;
				operation = Op::Extract;
				break;
			case 'E':
				opcount += 1;
				operation = Op::Extract;
				if (!optarg) abort();
				path = optarg;
				break;
			case 'n':
				sort_numerical = true;
				break;
			case 'p':
				sort_numerical_padding = true;
				break;
			case 's':
				strip_extensions = true;
				break;
			case 'b':
				skip_deleted = true;
				break;
			case 'X':
				add_extension = true;
				break;
			case 'P':
				convert_petscii = false;
				break;
			default:
				print_help();
				exit(1);
		}

	if (opcount > 1) {
		std::cout << "Only 1 action may be specified at a time." << std::endl;
		print_help();
		exit(1);
	}

	if (optind >= argc) {
		std::cout << "Missing input file(s)" << std::endl;
		print_help();
		exit(1);
	}
	std::string lbrfile = argv[optind++];
	std::vector<std::string> files;
	while (optind < argc) {
		files.push_back(argv[optind++]);
	}

	if (operation != Op::Create) {
		if (!std::filesystem::exists(lbrfile)) {
			std::cout << "File not found: " << lbrfile << std::endl;
			exit(1);
		}
	}

	if (operation != Op::Delete && operation != Op::Wipe) {
		for (const auto & a : files) {
			if (!std::filesystem::exists(a)) {
				std::cout << "File not found: " << a << std::endl;
				exit(1);
			}
		}
	}
	if (operation == Op::List || operation == Op::ChangeType) {
		if (!files.empty()) {
			std::cout << "Got extra unhandled arguments." << std::endl;
			print_help();
			exit(1);
		}
	}

	if (operation == Op::Create)
		build_lbr(lbrfile, files, sort_numerical, sort_numerical_padding, strip_extensions);
	else if (operation == Op::Extract)
		extract_lbr(lbrfile, path, files, skip_deleted, add_extension);
	else if (operation == Op::Delete)
		delete_lbr(lbrfile, target_file, skip_deleted, false);
	else if (operation == Op::Wipe)
		delete_lbr(lbrfile, target_file, skip_deleted, true);
	else if (operation == Op::Append)
		add_lbr(lbrfile, files, strip_extensions);
	else if (operation == Op::List)
		list_lbr(lbrfile, skip_deleted, sort_numerical);
	else if (operation == Op::ChangeType)
		chtype_lbr(lbrfile, target_file, new_type, skip_deleted);
	else {
		std::cout << "No action specified." << std::endl;
		print_help();
		exit(1);
	}
}

static void print_version()
{
	std::cout << "LBR Tool version 1.0" << std::endl;
	std::cout << "Copyright (c) 2020 Talas (talas.pw)." << std::endl;
	std::cout << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>" << std::endl;
	std::cout << "This is free software: you are free to change and redistribute it." << std::endl;
	std::cout << "There is NO WARRANTY, to the extent permitted by law." << std::endl;
}
