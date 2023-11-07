/*
	Psuedo Interactive SMALLF.DAT File Extractor

	LemonHaze - 28/10/2023
	================================================

	v1.0 - First release.
*/
#include <iostream>
#include <fstream>
#include <ostream>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include <string>
#include <filesystem>

using namespace std;
namespace fs = filesystem;

class SmallFile {
public:
	struct Entry {
		string path;
		int offs = 0;

		vector<unsigned char> data;
	};

	SmallFile() = default;
	SmallFile(fs::path in) {
		read(in);
	}

	void read(fs::path in)
	{
		ifstream ifs(in, ios::binary);

		int header_size = -1;
		ifs.read(reinterpret_cast<char*>(&header_size), 4);

		while (ifs.tellg() != header_size-2)
		{
			char path_len = 0xFFu;
			ifs.read(reinterpret_cast<char*>(&path_len), 1);

			path_len++;
			string path;
			while (path_len--)
				path.push_back(ifs.get());

			int offs = 0xFFFFFFFF;
			ifs.read(reinterpret_cast<char*>(&offs), 4);

			Entry entry;
			entry.path = path;
			entry.offs = offs;
			Entries.push_back(entry);
		}

		for (auto& entry : Entries)
		{
			while (ifs.tellg() < entry.offs)
				entry.data.push_back(ifs.get());
		}

		ifs.close();
	}	

	void extract(fs::path path)
	{
		for (const auto& entry : Entries)
		{
			fs::path new_path = path.string().append("/").append(entry.path);
			fs::create_directories(new_path.parent_path());

			ofstream ofs(new_path, ios::binary | ios::out);
			if (ofs.good())
			{
				ofs.write((char*)entry.data.data(), entry.data.size());
				ofs.close();
			}
		}
	}

	int calc_toc_size()
	{
		int toc_size = 4;
		for (const auto& entry : Entries)
		{
			toc_size++;								// space for path_len
			toc_size += (char)entry.path.size();	// string data
			toc_size++;								// null-byte
			toc_size += 4;							// offset

		}
		return toc_size + 2;
	}

	size_t create(fs::path outputDir)
	{
		for (const auto& p : fs::recursive_directory_iterator(outputDir)) {
			if (!fs::is_regular_file(p))
				continue;

			SmallFile::Entry entry;
			entry.path = fs::relative(p.path(), outputDir).string();
			printf("Adding '%s'... ", entry.path.c_str());

			ifstream ifs(p.path(), ios::binary);
			if (ifs.good())
			{
				ifs.seekg(0, ios::end);
				size_t eof = ifs.tellg();
				ifs.seekg(0, ios::beg);

				while ((size_t)ifs.tellg() < eof)
					entry.data.push_back(ifs.get());

				printf("(size: 0x%X).\n", (int)entry.data.size());
			}
			Entries.push_back(entry);
		}

		// just for niceness, we can sort by top-level items after any items in directories.
		auto depth_sort = [](const SmallFile::Entry& s1, const SmallFile::Entry& s2) {
			fs::path p1 = s1.path, p2 = s2.path;
			auto depth1 = distance(p1.begin(), p1.end()), depth2 = distance(p2.begin(), p2.end());
			if (depth1 == depth2)
				return p1 < p2;
			return depth2 < depth1;
		};
		sort(Entries.begin(), Entries.end(), depth_sort);

		return Entries.size();
	}

	bool write(fs::path path)
	{
		ofstream ofs(path, ios::binary | ios::out);
		if (ofs.good())
		{
			// write header
			int toc_size = calc_toc_size();
			ofs.write(reinterpret_cast<char*>(&toc_size), 4);

			// write out toc
			int last_written_block = toc_size-2;
			for (const auto& entry : Entries)
			{
				last_written_block += (int)entry.data.size();

				char path_len = (char)entry.path.size();
				ofs.write(reinterpret_cast<char*>(&path_len), 1);
				ofs.write(entry.path.c_str(), path_len);
				ofs.put(0);
				ofs.write(reinterpret_cast<char*>(&last_written_block), 4);
			}

			// write out all of the entries
			for (const auto& entry : Entries)
				ofs.write((char*)entry.data.data(), entry.data.size());

			// pad 'em out.
			int aligned_end = (((int)ofs.tellp()) + 2048 - 1) & ~(2048 - 1);
			while (ofs.tellp() < aligned_end)
				ofs.put(0);

			ofs.close();

			return true;
		}
		return false;
	}

	vector<Entry> Entries;
	vector<unsigned char> UnknownValues;
};

int main(int argc, char ** argp)
{
	printf("fextract - v1.0 - LemonHaze 2023\n\n");
	if (argc < 4 || argc > 5) {
		printf("fextract <e|c> <file> <input|output directory>\n");
		return -1;
	}

	fs::path data_dir = argp[3];
	if (argp[1][0] == 'e' || argp[1][0] == 'E')
	{
		fs::path input_filepath = argp[2];
		if (!fs::exists(input_filepath)) {
			printf("ERROR: The input file does not exist or is not able to be read.");
			return -2;
		}

		SmallFile file(input_filepath);
		if (file.Entries.size()) 
		{
			fs::create_directories(data_dir);
			file.extract(data_dir);
			printf("Extracted %zd entries from '%s'\n", file.Entries.size(), input_filepath.string().c_str());
		}
		else {
			printf("ERROR: No entries found.\n");
			return -3;
		}
	}
	if (argp[1][0] == 'c' || argp[1][0] == 'C')
	{
		if (!fs::exists(data_dir)) {
			printf("ERROR: The directory you specified is empty or does not exist!\n");
			return -2;
		}

		SmallFile created_file;
		if (created_file.create(data_dir))
		{
			fs::path input_outpath = argp[2];
			if (created_file.write(input_outpath))
				printf("Written %zd entries to '%s'\n", created_file.Entries.size(), input_outpath.string().c_str());
			else {
				printf("ERROR: No entries written.\n");
				return -4;
			}
		}
		else {
			printf("ERROR: No entries found.\n");
			return -3;
		}
	}
	return 0;
}