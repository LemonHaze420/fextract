# fextract
fextract is a CLI utility to extract and create archives based on Pseudo Interactive's "smallFile" archive file format.

### Usage
Extract `SMALLFILE.DAT` to `C:\files`:

	fextract e SMALLFILE.DAT C:\files
Create `SMALLFILE.DAT` from files in `C:\files`:

	fextract c SMALLFILE.DAT C:\files

### Format Specification
A basic archive format which stores a 32-bit integer of the size of the table of contents. 

Each item uses a byte for the path (limiting the path to 255 chars), followed by the path itself (+1 byte for the null-byte). 
After the path, the ending offset for the data is also stored in a 32-bit integer. 

After the table of contents, all file data is stored on top of eachother, sequentially and the resulting file is aligned to 2K boundaries.
