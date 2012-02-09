About
   Bget downloads HTTP URL with many network interfaces at the same time.
   The user has to assign interfaces and file's URL to bget.
   Bget splites the file into sections. How many sections, and the sections
   sizes are determined by the file size and how many interfaces it uses.
   When you use bget to download a file and the file size is 1030 bytes with 
   the following command:
      bget -i ip1 -i ip2 -o outputfile http://some.site.com/file
   The first file section from byte 0 to byte 511 is downloaded with ip1,
   and  the rest of the file is downloaded with ip2.
   The first section is saved in outputfilea, and the second section is saved in
   outputfileb.
   

Usage:
   bget [-i if_1] [-i if_2] [-s size] [-S size_force]-o file URL

   If the file size is not determinable, use -s to tell bget.
   Use -S to force bget to set the download file size.
