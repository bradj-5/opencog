/*
 * LGDictReader.cc
 *
 * Copyright (c) 2012, 2013 Linas Vepstas <linasvepstas@gmail.com> 
 * Copyright (C) 2014 OpenCog Foundation
 *
 * Author: Linas Vepstas <linasvepstas@gmail.com>
 *         William Ma <https://github.com/williampma>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <opencog/nlp/types/atom_types.h>

#include "LGDictReader.h"

using namespace opencog;


/**
 * Constructor of the LGDictReader class.
 *
 * @param pDict   the Dictionary to read from
 * @param pAS     the AtomSpace where atoms will be created
 */
LGDictReader::LGDictReader(Dictionary pDict, AtomSpace* pAS)
    : _dictionary(pDict), _as(pAS), _scm_eval(new SchemeEval(pAS))
{

}

/**
 * Destructor of the LGDictReader class.
 */
LGDictReader::~LGDictReader()
{
    delete _scm_eval;
}

/**
 * Method to construct LG dictionary atom.
 *
 * Given a word string, construct the corresponding atom representing
 * the Link Grammar's dictionary entry.
 *
 * The returned expression is in the form of an opencog-style
 * prefix-notation boolean expression.  Note that it is not in any
 * particular kind of normal form.  In particular, some AND nodes
 * may have only one child: these can be removed.
 *
 * Note that the order of the connectors is important: while linking,
 * these must be satisfied in left-to-right (nested!?) order.
 *
 * Optional clauses are indicated by OR-ing with null, where "null"
 * is a CONNECTOR Node with string-value "0".  Optional clauses are
 * not necessarily in any sort of normal form; the null connector can
 * appear anywhere.
 *
 * @param word   the input word string
 * @return       the handle to the newly created atom
 */
Handle LGDictReader::getAtom(const std::string& word)
{
    // See if we know about this word, or not.
    Dict_node* dn_head = dictionary_lookup_list(_dictionary, word.c_str());

    if (!dn_head)
        return Handle::UNDEFINED;

    HandleSeq outgoing;
    Handle hWord = _as->addNode(WORD_NODE, word);

    for (Dict_node* dn = dn_head; dn; dn = dn->right)
    {
        Exp* exp = dn->exp;
        Handle hLG = lg_exp_to_container(exp).to_handle(_as);

        outgoing.push_back(_as->addLink(LG_WORD_CSET, hWord, hLG));
    }

    free_lookup_list(_dictionary, dn_head);

    return _as->addLink(SET_LINK, outgoing);

#ifdef LG_OLD_SCM_METHOD
    std::string set = "(SetLink\n";

    for (Dict_node* dn = dn_head; dn; dn = dn->right)
    {
        Exp* exp = dn->exp;

        // First atom at the front of the outgoing set is the word itself.
        // Second atom is the first disjuct that must be fulfilled.
        std::string word_cset = " (LgWordCset (WordNode \"";
        word_cset += word;
        word_cset += "\")\n";
        word_cset += lg_exp_to_container(exp).to_scm_string();
        word_cset += ")\n";

        set += word_cset;
    }

    set += ")\n";

    free_lookup_list(_dictionary, dn_head);

    return _scm_eval->eval_h(set);
#endif
}

/**
 * Helper method for storing LG expression trees in custom container.
 *
 * @param exp   the input expression trees
 * @return      the flatten container
 */
LGDictExpContainer LGDictReader::lg_exp_to_container(Exp* exp)
{
    if (CONNECTOR_type == exp->type)
        return LGDictExpContainer(CONNECTOR_type, exp);

    // Whenever a null appears in an OR-list, it means the
    // entire OR-list is optional.  A null can never appear
    // in an AND-list.
    E_list* el = exp->u.l;

    if (NULL == el)
        return LGDictExpContainer(CONNECTOR_type, NULL);

    // The C data structure that link-grammar uses for connector
    // expressions is totally insane, as witnessed by the loop below.
    // Anyway: operators are infixed, i.e. are always binary,
    // with exp->u.l->e being the left hand side, and
    //      exp->u.l->next->e being the right hand side.
    // This means that exp->u.l->next->next is always null.
    std::vector<LGDictExpContainer> subcontainers;
    subcontainers.push_back(lg_exp_to_container(el->e));
    el = el->next;

    while (el && exp->type == el->e->type)
    {
        el = el->e->u.l;
        subcontainers.push_back(lg_exp_to_container(el->e));
        el = el->next;
    }

    if (el)
        subcontainers.push_back(lg_exp_to_container(el->e));

    return LGDictExpContainer(exp->type, subcontainers);
}
