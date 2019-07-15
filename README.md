# pspdftool
Preprinting preparation tool for PDF ebooks.  

### Build and Install
Enter directory src  
`cd src`  
run  
```
make -j4  
sudo make install  
```

This is a forked project of pspdftool by Ales Snuparek. Thanks to him for this wonderful project.  
The project has many bugs that I am trying to fix.  

### Changes over original project
* doesn't write negative page numbers  
* move page (translate) now works  
* can use even and odd page set in apply command  
* duplex mode is now working  
* scaleto bug is fixed  