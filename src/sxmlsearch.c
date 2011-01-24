/*
    This file is part of sxmlc.

    sxmlc is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sxmlc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with sxmlc.  If not, see <http://www.gnu.org/licenses/>.

	Copyright 2010 - Matthieu Labas
*/
#if defined(WIN32) || defined(WIN64)
#pragma warning(disable : 4996)
#endif

#include <string.h>
#include <stdlib.h>
#include "sxmlc.h"
#include "sxmlsearch.h"
#include "utils.h"

#define INVALID_XMLNODE_POINTER ((XMLNode*)-1)

/* The function used to compare a string to a pattern */
static REGEXPR_COMPARE regstrcmp_search = regstrcmp;

REGEXPR_COMPARE XMLSearch_set_regexpr_compare(REGEXPR_COMPARE fct)
{
	REGEXPR_COMPARE previous = regstrcmp_search;

	regstrcmp_search = fct;

	return previous;
}

int XMLSearch_init(XMLSearch* search)
{
	if (search == NULL) return false;

	if (search->init_value == XML_INIT_DONE) XMLSearch_free(search, true);

	search->tag = NULL;
	search->text = NULL;
	search->attributes = NULL;
	search->n_attributes = 0;
	search->next = NULL;
	search->prev = NULL;
	search->stop_at = INVALID_XMLNODE_POINTER; /* Because 'NULL' can be a valid value */

	return true;
}

int XMLSearch_free(XMLSearch* search, int free_next)
{
	int i;

	if (search == NULL || search->init_value != XML_INIT_DONE) return false;

	if (search->tag != NULL) {
		__free(search->tag);
		search->tag = NULL;
	}

	if (search->n_attributes > 0 && search->attributes != NULL) {
		for (i = 0; i < search->n_attributes; i++) {
			if (search->attributes[i].name != NULL) __free(search->attributes[i].name);
			if (search->attributes[i].value != NULL) __free(search->attributes[i].value);
		}
		__free(search->attributes);
		search->n_attributes = 0;
		search->attributes = NULL;
	}

	if (free_next && search->next != NULL) {
		(void)XMLSearch_free(search->next, true);
		search->next = NULL;
	}
	search->init_value = 0; /* Something not XML_INIT_DONE, otherwise we'll go into 'XMLSearch_free' again */
	(void)XMLSearch_init(search);

	return true;
}

int XMLSearch_search_set_tag(XMLSearch* search, const char* tag)
{
	if (search == NULL) return false;

	if (tag == NULL && search->tag != NULL) __free(search->tag);

	search->tag = __strdup(tag);

	return true;
}

int XMLSearch_search_set_text(XMLSearch* search, const char* text)
{
	if (search == NULL) return false;

	if (text == NULL && search->text != NULL) __free(search->text);

	search->text = __strdup(text);

	return true;
}

int XMLSearch_search_add_attribute(XMLSearch* search, const char* attr_name, const char* attr_value, int value_equal)
{
	int i, n;
	XMLAttribute* p;

	if (search == NULL) return -1;

	if (attr_name == NULL || attr_name[0] == '\0') return -1;

	n = search->n_attributes + 1;
	p = (XMLAttribute*)__realloc(search->attributes, n * sizeof(XMLAttribute));
	if (p == NULL) return -1;

	i = search->n_attributes;
	p[i].active = value_equal;
	p[i].name = __strdup(attr_name);
	if (p[i].name == NULL) {
		search->attributes = __realloc(p, i*sizeof(XMLAttribute)); /* Revert back to original size */
		return -1;
	}

	if (attr_value != NULL) {
		p[i].value = __strdup(attr_value);
		if (p[i].value == NULL) {
			__free(p[i].name);
			search->attributes = __realloc(p, i*sizeof(XMLAttribute)); /* Revert back to original size */
			return -1;
		}
	}

	search->n_attributes = n;
	search->attributes = p;

	return i;
}

int XMLSearch_search_get_attribute_index(const XMLSearch* search, const char* attr_name)
{
	int i;

	if (search == NULL || attr_name == NULL || attr_name[0] == '\0') return -1;

	for (i = 0; i < search->n_attributes; i++) {
		if (!strcmp(search->attributes[i].name, attr_name)) return i;
	}

	return -1;
}

int XMLSearch_search_remove_attribute(XMLSearch* search, int i_attr)
{
	if (search == NULL || i_attr < 0 || i_attr >= search->n_attributes) return -1;

	/* Free attribute fields first */
	if (search->attributes[i_attr].name != NULL) __free(search->attributes[i_attr].name);
	if (search->attributes[i_attr].value != NULL) __free(search->attributes[i_attr].value);

	memmove(&search->attributes[i_attr], &search->attributes[i_attr+1], (search->n_attributes - i_attr - 1) * sizeof(XMLAttribute));
	search->attributes = (XMLAttribute*)__realloc(search->attributes, --(search->n_attributes) * sizeof(XMLAttribute)); /* Frees memory */

	return search->n_attributes;
}

int XMLSearch_search_set_children_search(XMLSearch* search, XMLSearch* children_search)
{
	if (search == NULL) return false;

	if (search->next != NULL) XMLSearch_free(search->next, true);

	search->next = children_search;
	children_search->prev = search;

	return true;
}

char* XMLSearch_get_XPath_string(const XMLSearch* search, char** xpath, char quote)
{
	const XMLSearch* s;
	char squote[] = "'";
	int i, fill;

	if (xpath == NULL) return NULL;

	/* NULL 'search' is an empty string */
	if (search == NULL) {
		*xpath = __strdup("");
		if (*xpath == NULL) return NULL;

		return *xpath;
	}

	squote[0] = (quote == '\0' ? XML_DEFAULT_QUOTE : quote);

	for (s = search; s != NULL; s = s->next) {
		if (s != search) /* No "/" prefix for the first criteria */
			if (strcat_alloc(xpath, "/") == NULL) goto err;
		if (strcat_alloc(xpath, s->tag == NULL || s->tag[0] == '\0' ? "*": s->tag) == NULL) goto err;

		if (s->n_attributes > '\0' || (s->text != NULL && s->text[0] != '\0'))
			if (strcat_alloc(xpath, "[") == NULL) goto err;

		fill = false; /* '[' has not been filled with text yet, no ", " separator should be added */
		if (s->text != NULL && s->text[0] != '\0') {
			if (strcat_alloc(xpath, ".=") == NULL) goto err;
			if (strcat_alloc(xpath, squote) == NULL) goto err;
			if (strcat_alloc(xpath, s->text) == NULL) goto err;
			if (strcat_alloc(xpath, squote) == NULL) goto err;
			fill = true;
		}

		for (i = 0; i < s->n_attributes; i++) {
			if (fill) {
				if (strcat_alloc(xpath, ", ") == NULL) goto err;
			}
			else
				fill = true; /* filling is being performed */
			if (strcat_alloc(xpath, "@") == NULL) goto err;
			if (strcat_alloc(xpath, s->attributes[i].name) == NULL) goto err;
			if (s->attributes[i].value == NULL) continue;

			if (strcat_alloc(xpath, s->attributes[i].active ? "=" : "!=") == NULL) goto err;
			if (strcat_alloc(xpath, squote) == NULL) goto err;
			if (strcat_alloc(xpath, s->attributes[i].value) == NULL) goto err;
			if (strcat_alloc(xpath, squote) == NULL) goto err;
		}
		if ((s->text != NULL && s->text[0] != '\0') || s->n_attributes > 0) {
			if (strcat_alloc(xpath, "]") == NULL) goto err;
		}
	}

	return *xpath;

err:
	__free(*xpath);
	*xpath = NULL;

	return NULL;
}

/*
 Extract search information from 'xpath', where 'xpath' represents a single node
 (i.e. no '/' inside, except escaped ones), stripped from lead and tail '/'.
 tag[.=text, @attrib="value"] with potential spaces around '=' and ','.
 Return 'false' if parsing failed, 'true' for success.
 This is an internal function so we assume that arguments are valid (non-NULL).
 */
static int _init_search_from_1XPath(char* xpath, XMLSearch* search)
{
	char *p, *q;
	char c, c1, cc;
	int l0, l1, is, r0, r1;
	int ret;

	XMLSearch_init(search);

	/* Look for tag name */
	for (p = xpath; *p && *p != '['; p++) ;
	c = *p; /* Either '[' or '\0' */
	*p = 0;
	ret = XMLSearch_search_set_tag(search, xpath);
	*p = c;
	if (!ret) return false;

	if (*p == 0) return true;

	/* Here, '*p' is '[', we have to parse either text or attribute names/values until ']' */
	for (p++; *p && *p != ']'; p++) {
		for (q = p; *q && *q != ',' && *q != ']'; q++) ; /* Look for potential ',' separator to null it */
		cc = *q;
		if (*q == ',') *q = '\0';
		ret = true;
		switch (*p) {
			case '.': /* '.[ ]=[ ]["']...["']' to search for text */
				if (!split_left_right(p, '=', &l0, &l1, &is, &r0, &r1, true, true)) return false;
				c = p[r1+1];
				p[r1+1] = '\0';
				ret = XMLSearch_search_set_text(search, p+r0);
				p[r1+1] = c;
				p += r1+1;
				break;

			/* Attribute name, possibly '@attrib[[ ]=[ ]"value"]' */
			case '@':
				if (!split_left_right(p+1, '=', &l0, &l1, &is, &r0, &r1, true, true)) return false;
				c = p[l1+2];
				c1 = p[r1+2];
				p[l1+2] = '\0';
				p[r1+2] = '\0';
				ret = (XMLSearch_search_add_attribute(search, p+1+l0, (is < 0 ? NULL : p+1+r0), true) < 0 ? false : true); /* 'is' < 0 when there is no '=' (i.e. check for attribute presence only */
				p[l1+2] = c;
				p[r1+2] = c1;
				p += r1+1; /* Jump to next value */
				break;

			default: /* Not implemented */
				break;
		}
		*q = cc; /* Restore ',' separator if any */
		if (!ret) return false;
	}

	return true;
}

int XMLSearch_init_from_XPath(char* xpath, XMLSearch* search)
{
	XMLSearch *search1, *search2;
	char *p, *tag;
	char c;

	if (!XMLSearch_init(search)) return false;

	/* NULL or empty xpath is an empty (initialized only) search */
	if (xpath == NULL || *xpath == '\0') return true;

	search1 = NULL;		/* Search struct to add the xpath portion to */
	search2 = search;	/* Search struct to be filled from xpath portion */

	tag = xpath;
	while (*tag) {
		if (search2 != search) { /* Allocate a new search when the original one (i.e. 'search') has already been filled */
			search2 = (XMLSearch*)__malloc(sizeof(XMLSearch));
			if (search2 == NULL) {
				(void)XMLSearch_free(search, true);
				return false;
			}
		}
		/* Skip all first '/' */
		for (; *tag && *tag == '/'; tag++) ;
		if (*tag == '\0') return false; /* Only '/' */

		/* Look for the end of tag name: after '/' (to get another tag) or end of string */
		for (p = tag+1; *p && *p != '/'; p++) {
			if (*p == '\\' && *++p == '\0') break; /* Escape character, '\' could be the last character... */
		}
		c = *p; /* Backup character before nulling it */
		*p = '\0';
		if (!_init_search_from_1XPath(tag, search2)) {
			*p = c;
			(void)XMLSearch_free(search, true);
			return false;
		}
		*p = c;

		/* 'search2' is the newly parsed tag, 'search1' is the previous tag (or NULL if 'search2' is the first tag to parse (i.e. 'search2' == 'search') */

		if (search1 != NULL) search1->next = search2;
		if (search2 != search) search2->prev = search1;
		search1 = search2;
		search2 = NULL; /* Will force allocation during next loop */
		tag = p;
	}

	return true;
}

static int _attribute_matches(XMLAttribute* to_test, XMLAttribute* pattern)
{
	if (to_test == NULL && pattern == NULL) return true;

	if (to_test == NULL || pattern == NULL) return false;
	
	/* No test on name => match */
	if (pattern->name == NULL || pattern->name[0] == '\0') return true;

	/* Test on name fails => no match */
	if (!regstrcmp_search(to_test->name, pattern->name)) return false;

	/* No test on value => match */
	if (pattern->value == NULL) return true;

	/* Test on value according to pattern "equal" attribute */
	return regstrcmp_search(to_test->value, pattern->value) == pattern->active ? true : false;
}

int XMLSearch_node_matches(const XMLNode* node, const XMLSearch* search)
{
	int i, j;

	if (node == NULL) return false;

	if (search == NULL) return true;

	/* No comments, prolog, or such type of nodes are tested */
	if (node->tag_type != TAG_FATHER && node->tag_type != TAG_SELF) return false;

	/* Check tag */
	if (search->tag != NULL && !regstrcmp_search(node->tag, search->tag)) return false;

	/* Check text */
	if (search->text != NULL && !regstrcmp_search(node->text, search->text)) return false;

	/* Check attributes */
	if (search->attributes != NULL) {
		for (i = 0; i < search->n_attributes; i++) {
			for (j = 0; j < node->n_attributes; j++) {
				if (!node->attributes[j].active) continue;
				if (_attribute_matches(&node->attributes[j], &search->attributes[i])) break;
			}
			if (j >= node->n_attributes) return false; /* All attributes where scanned without a successful match */
		}
	}

	/* 'node' matches 'search'. If there is a father search, its father must match it */
	if (search->prev != NULL) return XMLSearch_node_matches(node->father, search->prev);

	/* TODO: Should a node match if search has no more 'prev' search and node father is still below the initial search ?
	 Depends if XPath started with "//" (=> yes) or "/" (=> no).
	 if (search->prev == NULL && node->father != search->from) return false; ? */
		
	return true;
}

XMLNode* XMLSearch_next(const XMLNode* from, XMLSearch* search)
{
	XMLNode* node;

	if (search == NULL || from == NULL) return NULL;

	/* Go down the last child search as fathers will be tested recursively by the 'XMLSearch_node_matches' function */
	for (; search->next != NULL; search = search->next) ;

	/* Initialize the 'stop_at' node on first search, to remember where to stop as there will be multiple calls */
	/* 'stop_at' can be NULL when 'from' is a root node, that is why it should be initialized with something else than NULL */
	if (search->stop_at == INVALID_XMLNODE_POINTER) search->stop_at = XMLNode_next_sibling(from);

	for (node = XMLNode_next(from); node != search->stop_at; node = XMLNode_next(node)) { /* && node != NULL */
		if (!XMLSearch_node_matches(node, search)) continue;

		/* 'node' is a matching node */

		/* No search to perform on 'node' children => 'node' is returned */
		if (search->next == NULL) return node;

		/* Run the search on 'node' children */
		return XMLSearch_next(node, search->next);
	}

	return NULL;
}

static char* _get_XPath(const XMLNode* node, char** xpath)
{
	int i, n, brackets;

	brackets = 0;
	n = strlen(node->tag);
	if (node->text != NULL) {
		n += strlen_html(node->text) + 4; /* 4 = '.=""' */
		brackets = 2; /* Text has to be displayed => add '[]' */
	}
	for (i = 0; i < node->n_attributes; i++) {
		if (!node->attributes[i].active) continue;
		brackets = 2; /* At least one attribute has to be displayed => add '[]' */
		n += strlen_html(node->attributes[i].name) + strlen_html(node->attributes[i].value) + 6; /* 6 = ', @=""' */
	}
	n += brackets;
	*xpath = (char*)__malloc(n+1);

	if (*xpath == NULL) return NULL;

	strcpy(*xpath, node->tag);
	if (node->text != NULL) {
		strcat(*xpath, "[.=\"");
		(void)str2html(node->text, *xpath+strlen(*xpath));
		strcat(*xpath, "\"");
		n = 1; /* Indicates '[' has been put */
	}
	else
		n = 0;

	for (i = 0; i < node->n_attributes; i++) {
		if (!node->attributes[i].active) continue;

		if (n == 0) {
			strcat(*xpath, "[");
			n = 1;
		}
		else
			strcat(*xpath, ", ");
		sprintf(*xpath+strlen(*xpath), "@%s=\"", node->attributes[i].name);
		(void)str2html(node->attributes[i].value, *xpath+strlen(*xpath));
		strcat(*xpath, "\"");
	}
	if (n > 0) strcat(*xpath, "]");

	return *xpath;
}

char* XMLNode_get_XPath(XMLNode* node, char** xpath, int incl_parents)
{
	char* xp = NULL;
	char* xparent;
	XMLNode* parent;

	if (node == NULL || node->init_value != XML_INIT_DONE || xpath == NULL) return NULL;

	if (!incl_parents) {
		if (_get_XPath(node, &xp) == NULL) {
			*xpath = NULL;
			return NULL;
		}
		return *xpath = xp;
	}

	/* Go up to root node */
	parent = node;
	do {
		xparent = NULL;
		if (_get_XPath(parent, &xparent) == NULL) goto xp_err;
		if (xp != NULL) {
			if (strcat_alloc(&xparent, "/") == NULL) goto xp_err;
			if (strcat_alloc(&xparent, xp) == NULL) goto xp_err;
		}
		xp = xparent;
		parent = parent->father;
	} while (parent != NULL);
	if ((*xpath = __strdup("/")) == NULL || strcat_alloc(xpath, xp) == NULL) goto xp_err;

	return *xpath;

xp_err:
	if (xp != NULL) __free(xp);
	*xpath = NULL;

	return NULL;
}