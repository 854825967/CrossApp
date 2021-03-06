

#ifndef __JSB_LOCALSTORAGE_H
#define __JSB_LOCALSTORAGE_H

#include <stdio.h>
#include <stdlib.h>

/** Initializes the database. If path is null, it will create an in-memory DB */
void localStorageInit( const char *fullpath);

/** Frees the allocated resources */
void localStorageFree();

/** sets an item in the LS */
void localStorageSetItem( const char *key, const char *value);

/** gets an item from the LS */
const char* localStorageGetItem( const char *key );

/** removes an item from the LS */
void localStorageRemoveItem( const char *key );

#endif // __JSB_LOCALSTORAGE_H
