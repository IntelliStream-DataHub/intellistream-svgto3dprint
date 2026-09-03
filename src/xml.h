/* Minimal XML DOM parser (enough for SVG documents). */
#ifndef LOGO3D_XML_H
#define LOGO3D_XML_H

#include <stddef.h>

typedef struct xml_attr {
    char *name;
    char *value;
} xml_attr;

typedef struct xml_node {
    char *name;                 /* tag name with namespace prefix stripped */
    xml_attr *attrs;
    int nattrs;
    struct xml_node **children;
    int nchildren;
    struct xml_node *parent;
    char *text;                 /* concatenated character data of this element */
} xml_node;

/* Parse an XML document. Returns the root element or NULL on error
 * (with a message in err). */
xml_node *xml_parse(const char *data, size_t len, char *err, size_t errlen);
void xml_free(xml_node *node);

/* Attribute lookup; returns NULL when absent. */
const char *xml_attr_get(const xml_node *node, const char *name);

#endif
