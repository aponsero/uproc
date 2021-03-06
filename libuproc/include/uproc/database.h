/* Copyright 2014 Peter Meinicke, Robin Martinjak, Manuel Landesfeind
 *
 * This file is part of libuproc.
 *
 * libuproc is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * libuproc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libuproc.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file uproc/database.h
 *
 * Module: \ref grp_datastructs_database
 *
 * \weakgroup grp_datastructs
 * \{
 *
 * \weakgroup grp_datastructs_database
 * \{
 */

#ifndef UPROC_DATABASE_H
#define UPROC_DATABASE_H

#include "uproc/ecurve.h"
#include "uproc/idmap.h"
#include "uproc/matrix.h"

/** \defgroup obj_database object uproc_database
 *
 * Database
 *
 * \{
 */

/** \struct uproc_database
 * \copybrief obj_database
 *
 * See \ref obj_database for details.
 */
typedef struct uproc_database_s uproc_database;

/**
  * Loads all required data of a UProC database from files in the given
  *directory and returns a database object.
  *
  * \param path an existing directory containing a UProC database.
  * \param prot_thres_level the protein threshold to be used. Note that the
  *corresponding file "prot_thresh_e%d" has to exist in the directory.
  * \param format the format of the ecurves
  *
  * \returns the object on success or %NULL on error
  */
uproc_database *uproc_database_load(const char *path, int prot_thresh_level,
                                    enum uproc_ecurve_format format);

/**
  * Returns the forward matching ecurve of the database. Note that the returned
  *object will
  * become invalid when the database is destroyed.
  *
  * \see uproc_database_destroy
  *
  * \param db the database.
  * \returns a pointer to the ecurve.
  */
uproc_ecurve *uproc_database_ecurve_forward(uproc_database *db);

/**
  * Returns the reverse matching ecurve of the database. Note that the returned
  *object will
  * become invalid when the database is destroyed.
  *
  * \see uproc_database_destroy
  *
  * \param db the database.
  * \returns a pointer to the ecurve.
  */
uproc_ecurve *uproc_database_ecurve_reverse(uproc_database *db);

/**
  * Returns the mapping from numerical to string IDs of the database. Note that
  *the returned object will
  * become invalid when the database is destroyed.
  *
  * \see uproc_database_destroy
  *
  * \param db the database.
  * \returns a pointer to the idmap.
  */
uproc_idmap *uproc_database_idmap(uproc_database *db);
/**
  * Returns the protein threshold matrix of the database. Note that the returned
  *object will
  * become invalid when the database is destroyed.
  * The returned pointer may be %NULL if the protein threshold was set to 0
  *while
  * loading the database.
  *
  * \see uproc_database_load
  * \see uproc_database_destroy
  *
  * \param db the database.
  * \returns a pointer to the matrix or %NULL
  */
uproc_matrix *uproc_database_protein_threshold(uproc_database *db);

/**
  * Destroy the database and all associated object within the database.
  * \param db the database to destroy and free memory for.
  */
void uproc_database_destroy(uproc_database *db);

#endif
