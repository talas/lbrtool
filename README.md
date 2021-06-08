# lbrtool
A tool to create, modify and extract from C64 LBR archives.

## Usage: lbr ACTION [OPTIONS] ARCHIVE [FILES...]
  -a : add files to the end of the archive  
  -l : print out entries in the archive (default action)  
  -c : create an archive with the given files  
  -e : extract from the archive  
  -E : extract from the archive, into the given FOLDER  
  -t : change filetype of a file in the archive to TYPE  
  -d : delete a file from the archive, keeping the entry  
  -w : delete a file from the archive completely  

$ ./lbr test.lbr  
BB.PRG (D) 0  
WW.TXT (S) 8  
HELLO.DAT (S) 12  
BB (P) 15  

# Compiling
Just call make.
You might not need to link with -lstdc++fs if your GCC is recent enough.

# License
GNU GPL v3 (or later), see LICENSE for more details.  
Recommended reading: https://www.gnu.org/licenses/quick-guide-gplv3.html
