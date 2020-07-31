# go-back-n-UDP

Description

This is a client/server application that uses go-back-n over UDP with a window
size of 7. The client reads a file containing a list of filenames, reads those
files, and sends them to server. The server compares the files to files stored
in a folder, and writes the filenames to a file, if it finds a match. If there
is no match, it writes the filename from client and <UNKNOWN>.

Go-back-N


Usage

