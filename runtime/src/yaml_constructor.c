#include <yaml_constructor.h>

#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>

char* yaml_constructor_escape(const char* const string, size_t* const size) {
	size_t needed = 0;
	for (const char* ptr = string; *ptr != '\0'; ++ptr) {
		needed += (*ptr == '\t' || *ptr == '\n' || *ptr == '\r' || *ptr == '\\' ||
			*ptr == '\"') ? 2 : 1;
	}
	char* res = malloc(needed + 3);
	*res = '\"';
	char* resptr = res + 1;
	if (size != NULL) *size = needed;
	for (const char* ptr = string; *ptr != '\0'; ptr++) {
		switch (*ptr) {
		case '\t': *resptr++ = '\\'; *resptr++ = 't'; break;
		case '\n': *resptr++ = '\\'; *resptr++ = 'n'; break;
		case '\r': *resptr++ = '\\'; *resptr++ = 'r'; break;
		case '\\': *resptr++ = '\\'; *resptr++ = '\\'; break;
		case '\"': *resptr++ = '\\'; *resptr++ = '\"'; break;
		default: *resptr++ = *ptr; break;
		}
	}
	*resptr++ = '\"';
	*resptr = '\0';
	return res;
}

const char* yaml_constructor_event_spelling(yaml_event_type_t type) {
	switch (type) {
	case YAML_STREAM_START_EVENT:   return "STREAM_START";
	case YAML_STREAM_END_EVENT:     return "STREAM_END";
	case YAML_DOCUMENT_START_EVENT: return "DOCUMENT_START";
	case YAML_DOCUMENT_END_EVENT:   return "DOCUMENT_END";
	case YAML_MAPPING_START_EVENT:  return "MAPPING_START";
	case YAML_MAPPING_END_EVENT:    return "MAPPING_END";
	case YAML_SEQUENCE_START_EVENT: return "SEQUENCE_START";
	case YAML_SEQUENCE_END_EVENT:   return "SEQUENCE_END";
	case YAML_SCALAR_EVENT:         return "SCALAR_EVENT";
	case YAML_ALIAS_EVENT:          return "ALIAS_EVENT";
	default:                        return "NO_EVENT";
	}
}

#define PO10_LIMIT (SIZE_MAX/10)

static size_t digits_count(size_t i) {
	size_t n = 1;
	size_t po10 = 10;
	while (i >= po10) {
		n++;
		if (po10 > PO10_LIMIT) break;
		po10 *= 10;
	}
	return n;
}

char* yaml_constructor_render_error(yaml_event_t *event, const char *message,
	size_t expected_param_length, ...) {
	static const char pos_template[] = "l. %zu, c. %zu: ";
	size_t expected_pos_len = sizeof(pos_template) - 7 + // placeholder + terminator
		digits_count(event->start_mark.line + 1) +
		digits_count(event->start_mark.column + 1);
	char* buffer = malloc(expected_pos_len + expected_param_length +
		strlen(message) + 1);
	int pos_len = sprintf(buffer, pos_template, event->start_mark.line,
		event->start_mark.column);
	assert(pos_len == expected_pos_len);
	va_list args;
	va_start(args, expected_param_length);
	vsprintf(buffer + expected_pos_len, message, args);
	va_end(args);
	return buffer;
}

char* yaml_constructor_wrong_event_error(yaml_event_type_t expected,
	yaml_event_t* actual) {
	return yaml_constructor_render_error(actual,
		"expected %s, got %s", 14 + 14,
		yaml_constructor_event_spelling(expected),
		yaml_constructor_event_spelling(actual->type));
}

#define DEFINE_INT_CONSTRUCTOR(name, value_type, min, max)\
char* name(value_type *const value, yaml_parser_t *const parser,\
                  yaml_event_t *cur) {\
  (void)parser;\
  if (cur->type != YAML_SCALAR_EVENT) {\
    return yaml_constructor_wrong_event_error(YAML_SCALAR_EVENT, cur);\
  }\
  char* result;\
  long long res = strtoll((const char*)cur->data.scalar.value, &result, 10);\
  if (*result != '\0') {\
    size_t escaped_len;\
    char* escaped = yaml_constructor_escape(\
        (const char*)cur->data.scalar.value, &escaped_len);\
    char* buffer = yaml_constructor_render_error(cur, \
        "cannot read %s as int!", escaped_len, escaped);\
    free(escaped);\
    return buffer;\
  } else if (res < min || res > max) {\
    size_t escaped_len;\
    char* escaped = yaml_constructor_escape(\
        (const char*)cur->data.scalar.value, &escaped_len);\
    char* buffer = yaml_constructor_render_error(cur,\
        "int value of %s outside representable range!",\
        escaped_len, escaped);\
    free(escaped);\
    return buffer;\
  }\
  *value = (value_type)res;\
  return NULL;\
}

DEFINE_INT_CONSTRUCTOR(yaml_construct_short, short, SHRT_MIN, SHRT_MAX)
DEFINE_INT_CONSTRUCTOR(yaml_construct_int, int, INT_MIN, INT_MAX)
DEFINE_INT_CONSTRUCTOR(yaml_construct_long, long, LONG_MIN, LONG_MAX)
DEFINE_INT_CONSTRUCTOR(yaml_construct_long_long, long long, LLONG_MIN,
	LLONG_MAX)

#define DEFINE_UNSIGNED_CONSTRUCTOR(name, value_type, max) \
char* name(value_type *const value, yaml_parser_t *const parser,\
                  yaml_event_t* cur) {\
  (void)parser;\
  if (cur->type != YAML_SCALAR_EVENT) {\
    return yaml_constructor_wrong_event_error(YAML_SCALAR_EVENT, cur);\
  }\
  char* result;\
  unsigned long long res =\
      strtoull((const char*)cur->data.scalar.value, &result, 10);\
  if (*result != '\0') {\
    size_t escaped_len;\
    char *escaped = yaml_constructor_escape(\
        (const char *) cur->data.scalar.value, &escaped_len);\
    char *buffer = yaml_constructor_render_error(\
        cur, "cannot read %s as " #value_type "!",\
        escaped_len, escaped);\
    free(escaped);\
    return buffer;\
  } else if (res > (max)) {\
    size_t escaped_len;\
    char* escaped = yaml_constructor_escape(\
        (const char*)cur->data.scalar.value, &escaped_len);\
    char* buffer = yaml_constructor_render_error(\
        cur, "size_t value of %s outside representable range!",\
        escaped_len, escaped);\
    free(escaped);\
    return buffer;\
  }\
  *value = (value_type)res;\
  return NULL;\
}

DEFINE_UNSIGNED_CONSTRUCTOR(yaml_construct_unsigned_char, unsigned char,
	UCHAR_MAX)
DEFINE_UNSIGNED_CONSTRUCTOR(yaml_construct_unsigned_short, unsigned short,
	USHRT_MAX)
DEFINE_UNSIGNED_CONSTRUCTOR(yaml_construct_unsigned, unsigned, UINT_MAX)
DEFINE_UNSIGNED_CONSTRUCTOR(yaml_construct_unsigned_long, unsigned long,
	ULLONG_MAX)
DEFINE_UNSIGNED_CONSTRUCTOR(yaml_construct_unsigned_long_long,
	unsigned long long, ULLONG_MAX)

 char* yaml_construct_string(char** const value, yaml_parser_t *const parser,
		yaml_event_t* cur) {
	(void)parser;
	if (cur->type != YAML_SCALAR_EVENT) {
		return yaml_constructor_wrong_event_error(YAML_SCALAR_EVENT, cur);
	}
	size_t len = strlen((char*)cur->data.scalar.value) + 1;
	*value = malloc(len);
	memcpy(*value, cur->data.scalar.value, len);
	return NULL;
}

char* yaml_construct_char(char *const value, yaml_parser_t *const parser,
	yaml_event_t* cur) {
	(void)parser;
	if (cur->type != YAML_SCALAR_EVENT) {
		return yaml_constructor_wrong_event_error(YAML_SCALAR_EVENT, cur);
	}
	else if (cur->data.scalar.value[0] == '\0' ||
		cur->data.scalar.value[1] != '\0') {
		size_t escaped_len;
		char* escaped = yaml_constructor_escape(
			(const char*)cur->data.scalar.value, &escaped_len);
		char* buffer = yaml_constructor_render_error(
			cur, "expected single character, got %s",
			escaped_len, escaped);
		free(escaped);
		return buffer;
	}
	*value = cur->data.scalar.value[0];
	return NULL;
}

char* yaml_construct_bool(bool *const value, yaml_parser_t *const parser,
	yaml_event_t* cur) {
	(void)parser;
	if (cur->type != YAML_SCALAR_EVENT) {
		return yaml_constructor_wrong_event_error(YAML_SCALAR_EVENT, cur);
	}
	else if (strcmp("true", (const char*)cur->data.scalar.value) == 0) {
		*value = true;
	}
	else if (strcmp("false", (const char*)cur->data.scalar.value) == 0) {
		*value = false;
	}
	else {
		size_t escaped_len;
		char* escaped = yaml_constructor_escape(
			(const char*)cur->data.scalar.value, &escaped_len);
		char* buffer = yaml_constructor_render_error(
			cur, "expected boolean value, got %s",
			escaped_len, escaped);
		free(escaped);
		return buffer;
	}
	return NULL;
}

#define DEFINE_FP_CONSTRUCTOR(name, value_type, overflow, func) \
char* name(value_type *const value, yaml_parser_t *const parser,\
                  yaml_event_t* cur) {\
  (void)parser;\
  if (cur->type != YAML_SCALAR_EVENT) {\
    return yaml_constructor_wrong_event_error(YAML_SCALAR_EVENT, cur);\
  }\
  char* end_ptr;\
  *value = func((const char*)cur->data.scalar.value, &end_ptr);\
  if (*end_ptr != '\0' || *value == (overflow)) {\
    size_t escaped_len;\
    char* escaped = yaml_constructor_escape(\
        (const char*)cur->data.scalar.value, &escaped_len);\
    char* buffer = yaml_constructor_render_error(\
        cur, "cannot parse as " #value_type \
        " value: %s", escaped_len, escaped);\
    free(escaped);\
    return buffer;\
  }\
  return NULL;\
}

DEFINE_FP_CONSTRUCTOR(yaml_construct_float, float, HUGE_VALF, strtof)
DEFINE_FP_CONSTRUCTOR(yaml_construct_double, double, HUGE_VAL, strtod)
DEFINE_FP_CONSTRUCTOR(yaml_construct_long_double, long double, HUGE_VALL,
	strtold)