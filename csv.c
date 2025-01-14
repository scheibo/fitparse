/*
 *  Copyright (c) 2014 Kirk Scheibelhut <kjs@scheibo.com>
 *
 *  This file is free software: you may copy, redistribute and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation, either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This file is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "csv.h"
#include "util.h"

/**
 * DATA_FIELDS
 *
 * Description:
 *  Mapping from `DataField` to field name string.
 */
static const char *DATA_FIELDS[DataFieldCount] = {
    "timestamp",  "latitude", "longitude",  "altitude",
    "distance",   "speed",    "power",      "grade",
    "heart_rate", "cadence",  "lr_balance", "temperature"};

/**
 * name_to_field
 *
 * Description:
 *  Fairly relaxed ,apping between a field name and a `DataField`.
 *  Ignores case and leading spaces and allows for multiple variations.
 *
 * Parameters:
 *  name - the field name to map.
 *
 * Return value:
 *  valid DataField - successfully mapped `name` to `DataField`.
 *  DataFieldCount - unable to match `name` to a field.
 */
static DataField name_to_field(char *name) {
  /* deal with optional leading whitespace */
  while (isspace(*name)) name++;
  /* make the name lowercase for comparisons */
  downcase(name);

  if (!strcmp(name, "timestamp") || !strcmp(name, "time")) {
    return Timestamp;
  } else if (!strcmp(name, "latitude") || !strcmp(name, "lat")) {
    return Latitude;
  } else if (!strcmp(name, "longitude") || !strcmp(name, "lon") ||
             !strcmp(name, "long")) {
    return Longitude;
  } else if (!strcmp(name, "altitude") || !strcmp(name, "elevation") ||
             !strcmp(name, "alt") || !strcmp(name, "ele")) {
    return Altitude;
  } else if (!strcmp(name, "distance") || !strcmp(name, "dist")) {
    return Distance;
  } else if (!strcmp(name, "speed") || !strcmp(name, "spd")) {
    return Speed;
  } else if (!strcmp(name, "power") || !strcmp(name, "pow") ||
             !strcmp(name, "watts")) {
    return Power;
  } else if (!strcmp(name, "slope") || !strcmp(name, "grade") ||
             !strcmp(name, "gradient")) {
    return Grade;
  } else if (!strcmp(name, "heart_rate") || !strcmp(name, "hr")) {
    return HeartRate;
  } else if (!strcmp(name, "cadence") || !strcmp(name, "cad")) {
    return Cadence;
  } else if (!strcmp(name, "balance") || !strcmp(name, "bal") ||
             !strcmp(name, "lr_balance")) {
    return LRBalance;
  } else if (!strcmp(name, "temperature") || !strcmp(name, "atemp") ||
             !strcmp(name, "temp")) {
    return Temperature;
  } else { /* not found */
    return DataFieldCount;
  }
}

/**
 * read_csv_header
 *
 * Description:
 *  Reads a CSV header and fills in an array of `DataField`s which provides a
 *  mapping between CSV field and `DataField`. We will only read at most
 *  `DataFieldCount` actual data fields from the CSV, and the fields may be
 *  duplicated (the last value takes precedence).
 *
 * Parameters:
 *  f - the file descriptor for the CSV file to read.
 *  data_fields - an array which is built up by the function that determines
 *                the order of `DataField`s within the CSV.
 *
 * Return value:
 *  0 - unable to read in the header
 *  1 - the number of `DataField`s within `data_fields`.
 */
static int read_csv_header(FILE *f, DataField data_fields[]) {
  char buf[CSV_BUFSIZ], *comma, *last, field_str[CSV_FIELD_SIZE];
  DataField field;
  unsigned i, count = 0;

  /* make sure we can read the header */
  if (!fgets(buf, sizeof(buf), f)) {
    return 0;
  }

  /* read in the header */
  for (i = 0, last = buf, comma = strchr(buf, ',');
       count < DataFieldCount && i < CSV_MAX_FIELDS && comma;
       comma = strchr(last, ','), i++) {
    strncpy(field_str, last, comma - last);
    field_str[comma - last] = '\0';

    if ((field = name_to_field(field_str)) != DataFieldCount) {
      count++;
    }
    data_fields[i] = field;

    last = comma + 1;
    memset(field_str, '\0', CSV_FIELD_SIZE);
  }
  /* strip newline */
  if ((comma = strrchr(last, '\n')) != NULL) *comma = '\0';
  /* grab the last header field (no trailing comma) */
  if (count < DataFieldCount && i < CSV_MAX_FIELDS) {
    if ((field = name_to_field(last)) != DataFieldCount) {
      count++;
    }
    data_fields[i] = field;
  }
  memset(buf, '\0', CSV_BUFSIZ);

  return count;
}

/**
 * read_csv_data
 *
 * Description:
 *
 * Parameters:
 *  f - the file descriptor for the CSV file to read.
 *  data_fields - the array built by `read_csv_headers` mapping CSV field to
 *                `DataField`.
 *  count - the number of `DataField`s in `data_fields`.
 *  a - the `Activity` to read the data into.
 */
static void read_csv_data(FILE *f, DataField data_fields[], unsigned count,
                          Activity *a) {
  char buf[CSV_BUFSIZ], *comma, *last, field_str[CSV_FIELD_SIZE];
  unsigned i, j;
  DataPoint dp;

  /* initialize */
  unset_data_point(&dp);

  /* read in data points */
  while (fgets(buf, sizeof(buf), f)) {
    for (i = 0, j = 0, last = buf, comma = strchr(buf, ',');
         j < count && i < CSV_MAX_FIELDS && comma;
         comma = strchr(last, ','), i++) {

      if (data_fields[i] != DataFieldCount) {
        strncpy(field_str, last, comma - last);
        field_str[comma - last] = '\0';
        parse_field(data_fields[i], &dp, field_str);
        memset(field_str, '\0', CSV_FIELD_SIZE);
        j++;
      }

      last = comma + 1;
    }
    /* strip newline */
    if ((comma = strrchr(last, '\n')) != NULL) *comma = '\0';
    /* grab the last header field (no trailing comma) */
    if (j < count && i < CSV_MAX_FIELDS && data_fields[i] != DataFieldCount) {
      strncpy(field_str, last, comma - last);
      field_str[comma - last] = '\0';
      parse_field(data_fields[i], &dp, field_str);
      memset(field_str, '\0', CSV_FIELD_SIZE);
    }

    activity_add_point(a, &dp);
    unset_data_point(&dp);
    memset(buf, '\0', CSV_BUFSIZ);
  }
}

/**
 * csv_read
 *
 * Description:
 *  Read in the CSV file pointed to by `f` and return an `Activity`.
 *  Fields can have several names and be in different orders, but we assume
 *  all are doubles in base SI unit which we then convert into our format.
 *
 * Parameters:
 *  f - the file descriptor for the CSV file to read.
 *
 * Return value:
 *  NULL - unable to read in CSV or invalid CSV file.
 *  valid pointer - a valid pointer to a newly allocated Activity instance.
 *                  The caller is responsible for freeing the activity.
 */
Activity *csv_read(FILE *f) {
  DataField data_fields[CSV_MAX_FIELDS];
  Activity *a;
  unsigned count;

  if (!(count = read_csv_header(f, data_fields))) return NULL;

  a = activity_new();
  read_csv_data(f, data_fields, count, a);
  a->format = CSV;

  return a;
}

/**
 * write_field
 *
 * Description:
 *  Writes a given data field of the `Activity` to `f` as allowed by `o`.
 *
 * Parameters:
 *  f - the file descriptor for the CSV file to write to.
 *  format - the format string to display the field in.
 *  i - the current data point being written.
 *  field - the field to write.
 *  a - the `Activity` to write.
 *  o - the options to use when printing the `Activity`.
 *  first - a pointer to a boolean determining whether this is the first
 *          written field of a given `DataPoint`.
 */
static void write_field(FILE *f, const char *format, size_t i, DataField field,
                        Activity *a, CSVOptions *o, bool *first) {
  double d = a->data_points[i].data[field];
  if (!o->remove_unset || a->last[field]) {
    if (!*first) {
      fprintf(f, ",");
    }
    if (!SET(d)) {
      fprintf(f, "%s", o.unset_value);
    } else {
      fprintf(f, format, d);
    }
    *first = false;
  }
}

/**
 * csv_write_options
 *
 * Description:
 *  Write the `Activity` to `f` in CSV format given the options provided.
 *
 * Parameters:
 *  f - the file descriptor for the CSV file to write to.
 *  a - the `Activity` to write.
 *  o - the options to use when writing the `Activity`.
 *
 * Return value:
 *  0 - successfully wrote CSV file.
 *  1 - unable to write CSV.
 */
int csv_write_options(FILE *f, Activity *a, CSVOptions *o) {
  unsigned i;
  bool first = true;

  assert(a != NULL);

  /* print header */
  for (i = 0; i < DataFieldCount; i++) {
    if (!o.remove_unset || a->last[i]) {
      if (first) {
        fprintf(f, "%s", DATA_FIELDS[i]);
        first = false;
      } else {
        fprintf(f, ",%s", DATA_FIELDS[i]);
      }
    }
  }
  fprintf(f, "\n");

  /* print data points - must be at least one non empty */
  for (i = 0, first = true; i < a->num_points; i++, first = true) {
    write_field(f, "%.0f", i, Timestamp, a, o, &first);
    write_field(f, "%.7f", i, Latitude, a, o, &first);
    write_field(f, "%.7f", i, Longitude, a, o, &first);
    write_field(f, "%.3f", i, Altitude, a, o, &first);
    write_field(f, "%.2f", i, Distance, a, o, &first);
    write_field(f, "%.2f", i, Speed, a, o, &first);
    write_field(f, "%.0f", i, Power, a, o, &first);
    write_field(f, "%.2f", i, Grade, a, o, &first);
    write_field(f, "%.0f", i, HeartRate, a, o, &first);
    write_field(f, "%.0f", i, Cadence, a, o, &first);
    write_field(f, "%.0f", i, LRBalance, a, o, &first);
    write_field(f, "%.0f", i, Temperature, a, o, &first);
    fprintf(f, "\n");
  }

  return 0;
}
