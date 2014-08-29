/*
 * Copyright (C) 2013 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

#include "JSONBuilderAgent.h"
using namespace std;

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
JSONBuilderAgent::JSONBuilderAgent(void)
{
	m_builder = json_builder_new();
}

JSONBuilderAgent::~JSONBuilderAgent()
{
	if (m_builder)
		g_object_unref(m_builder);
}

string JSONBuilderAgent::generate(void)
{
	JsonGenerator *generator = json_generator_new();
	JsonNode *root = json_builder_get_root(m_builder);
	json_generator_set_root(generator, root);

	gchar *str = json_generator_to_data(generator, NULL);
	string json_str = str;

	g_free(str);
	json_node_free(root);
	g_object_unref(generator);

	return json_str;
}

void JSONBuilderAgent::startObject(const char *member)
{
	if (member)
		json_builder_set_member_name(m_builder, member);
	json_builder_begin_object(m_builder);
}

void JSONBuilderAgent::startObject(const string &member)
{
        startObject(member.c_str());
}

void JSONBuilderAgent::endObject(void)
{
	json_builder_end_object(m_builder);
}

void JSONBuilderAgent::startArray(const string &member)
{
	json_builder_set_member_name(m_builder, member.c_str());
	json_builder_begin_array(m_builder);
}

void JSONBuilderAgent::endArray(void)
{
	json_builder_end_array(m_builder);
}

void JSONBuilderAgent::addNull(const string &member)
{
	json_builder_set_member_name(m_builder, member.c_str());
	json_builder_add_null_value(m_builder);
}

void JSONBuilderAgent::add(const string &member, const string &value)
{
	json_builder_set_member_name(m_builder, member.c_str());
	json_builder_add_string_value(m_builder, value.c_str());
}

void JSONBuilderAgent::add(const string &member, gint64 value)
{
	json_builder_set_member_name(m_builder, member.c_str());
	json_builder_add_int_value(m_builder, value);
}

void JSONBuilderAgent::add(const gint64 value)
{
	json_builder_add_int_value(m_builder, value);
}

void JSONBuilderAgent::add(const string &value)
{
	json_builder_add_string_value(m_builder, value.c_str());
}

void JSONBuilderAgent::addTrue(const string &member)
{
	json_builder_set_member_name(m_builder, member.c_str());
	json_builder_add_boolean_value(m_builder, TRUE);
}

void JSONBuilderAgent::addFalse(const string &member)
{
	json_builder_set_member_name(m_builder, member.c_str());
	json_builder_add_boolean_value(m_builder, FALSE);
}
