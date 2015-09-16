#include "../parse.h"

static unsigned parse_stmt_if__computed(
	const sparse_t* src, const char* ptr,
	parse_stmt_t* stmt)
{
	unsigned i = 0;

	if (ptr[i++] != '(')
		return 0;

	unsigned len = parse_expr(
		src, &ptr[i], &stmt->if_comp.cond);
	if (len == 0) return 0;
	i += len;

	if (ptr[i++] != ')')
	{
		parse_expr_cleanup(
			stmt->if_comp.cond);
		return 0;
	}

	parse_label_t label;
	len = parse_label(
		src, &ptr[i], &label);
	if (len == 0)
	{
		parse_expr_cleanup(
			stmt->if_comp.cond);
		return 0;
	}
	i += len;

	stmt->type = PARSE_STMT_IF_COMPUTED;

	stmt->if_comp.label_count = 1;
	stmt->if_comp.label = (parse_label_t*)malloc(
		sizeof(parse_label_t) * stmt->if_comp.label_count);
	if (!stmt->if_comp.label)
	{
		parse_expr_cleanup(
			stmt->if_comp.cond);
		return 0;
	}
	stmt->if_comp.label[0] = label;

	while (ptr[i] == ',')
	{
		unsigned j = (i + 1);
		len = parse_label(
			src, &ptr[j], &label);
		if (len == 0) break;

		parse_label_t* nlabel = (parse_label_t*)realloc(stmt->if_comp.label,
			sizeof(parse_label_t) * (stmt->if_comp.label_count + 1));
		if (!nlabel)
		{
			parse_stmt_cleanup(*stmt);
			return 0;
		}
		stmt->if_comp.label = nlabel;
		stmt->if_comp.label[stmt->if_comp.label_count++] = label;
		i = (j + len);
	}

	return i;
}

static unsigned parse_stmt_if__statement(
	const sparse_t* src, const char* ptr,
	const parse_implicit_t* implicit,
	hashmap_t* decl,
	parse_stmt_t* stmt)
{
	unsigned i = 0;

	if (ptr[i++] != '(')
		return 0;

	unsigned len = parse_expr(
		src, &ptr[i], &stmt->if_stmt.cond);
	if (len == 0) return 0;
	i += len;

	if (ptr[i++] != ')')
	{
		parse_expr_cleanup(
			stmt->if_stmt.cond);
		return 0;
	}

	parse_stmt_t cstmt;
	len = parse_stmt(
		src, &ptr[i], NULL,
		implicit, decl, &cstmt);
	if (len == 0)
	{
		parse_expr_cleanup(
			stmt->if_stmt.cond);
		return 0;
	}

	/* Don't absorb the end of statement here. */
	if (ptr[i + len] != '\0')
		len--;
	i += len;

	stmt->if_stmt.stmt
		= parse_stmt_alloc(cstmt);
	if (!stmt->if_stmt.stmt)
	{
		parse_stmt_cleanup(cstmt);
		parse_expr_cleanup(
			stmt->if_stmt.cond);
		return 0;
	}

	stmt->type = PARSE_STMT_IF_STATEMENT;
	return i;
}

unsigned parse_stmt_if(
	const sparse_t* src, const char* ptr,
	const parse_implicit_t* implicit,
	hashmap_t* decl,
	parse_stmt_t* stmt)
{
	unsigned i = parse_keyword(
		src, ptr, PARSE_KEYWORD_IF);
	if (i == 0) return 0;

	unsigned len = 0;
	if (len == 0) len = parse_stmt_if__statement(src, &ptr[i], implicit, decl, stmt);
	if (len == 0) len = parse_stmt_if__computed(src, &ptr[i], stmt);

	return (len == 0 ? 0 : (i + len));
}
