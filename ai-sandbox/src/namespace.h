#ifndef NAMESPACE_H
#define NAMESPACE_H

// Create a new mount namespace for the current process
int create_mount_namespace(void);

// Hide a directory inside the mount namespace
int hide_directory(const char *path);

#endif
