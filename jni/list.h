#pragma once

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

struct plist_head {
    struct list_head node_list;
};

struct plist_node {
    int prio;
    struct list_head prio_list;
    struct list_head node_list;
};

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#ifndef container_of
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#ifndef list_entry
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)
#endif
