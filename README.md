# pdfcook
Preprinting preparation tool for PDF ebooks.  

### Build and Install
Enter directory src  
`cd src`  
run  
```
make -j4  
sudo make install  
```  
Install manpage  
`sudo make installman`  

**Windows Build**  
On windows create a folder build/ beside src/ directory.  
And edit Makefile and remove lines with  
`	@mkdir -p $(@D)`  
Then to build run...  
 `make -j4`  

### Features
* PDF v1.7 support  
* Decrypt encrypted PDFs  
* Join or Split PDFs  
* Scale to any paper size, with specified margin  
* Write Page numbers  
* Write text  
* Transform pages (rotate, flip, move)  
* Booklet format arrange  
* 2 or 4 pages per page (2-up, 4-up)  
* More readable output syntax for easy debugging  

### Usage
See manual page (PDF or man page) for detailed usage  

Scale to print in A4 size paper  
`pdfcook 'scaleto(a4)' input.pdf output.pdf`  

Add binding margin after scaling (? for odd pages, + for even pages)  
`pdfcook 'scaleto(a4) move(20){?} move(-20){+}' input.pdf output.pdf`  

Add page numbers  
`pdfcook 'number' input.pdf output.pdf`  

Booklet format  
`pdfcook 'book nup(2, paper=a4)' input.pdf output.pdf`  
