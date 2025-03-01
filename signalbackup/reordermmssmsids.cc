/*
  Copyright (C) 2021-2023  Selwin van Dijk

  This file is part of signalbackup-tools.

  signalbackup-tools is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  signalbackup-tools is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with signalbackup-tools.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "signalbackup.ih"

bool SignalBackup::reorderMmsSmsIds() const
{
  std::cout << __FUNCTION__ << std::endl;

  // get all mms in the correct order
  SqliteDB::QueryResults res;
  if (!d_database.exec("SELECT _id FROM " + d_mms_table + " ORDER BY date_received ASC", &res)) // for sms table, use 'date'
    return false;

  // set all id's 'negatively ascending' (negative because of UNIQUE constraint)
  long long int negative_id_tmp = 0;
  for (uint i = 0; i < res.rows(); ++i)
  {
    long long int oldid = res.getValueAs<long long int>(i, 0);
    ++negative_id_tmp;
    if (!d_database.exec("UPDATE " + d_mms_table + " SET _id = ? WHERE _id = ?", {-1 * negative_id_tmp, oldid}) ||
        !d_database.exec("UPDATE part SET mid = ? WHERE mid = ?", {-1 * negative_id_tmp, oldid}) ||
        !d_database.exec("UPDATE group_receipts SET mms_id = ? WHERE mms_id = ?", {-1 * negative_id_tmp, oldid}))
      return false;
    if (d_database.containsTable("mention"))
      if (!d_database.exec("UPDATE mention SET message_id = ? WHERE message_id = ?", {-1 * negative_id_tmp, oldid}))
        return false;
    if (d_database.containsTable("msl_message"))
      if (!d_database.exec("UPDATE msl_message SET message_id = ? WHERE message_id = ?"s + (d_database.tableContainsColumn("msl_message", "is_mms") ? " AND is_mms IS 1" : ""), {-1 * negative_id_tmp, oldid}))
        return false;
    if (d_database.containsTable("reaction")) // dbv >= 121
      if (!d_database.exec("UPDATE reaction SET message_id = ? WHERE message_id = ?"s + (d_database.tableContainsColumn("reaction", "is_mms") ? " AND is_mms IS 1" : ""), {-1 * negative_id_tmp, oldid}))
        return false;
    if (d_database.containsTable("story_sends"))
      if (!d_database.exec("UPDATE story_sends SET message_id = ? WHERE message_id = ?", {-1 * negative_id_tmp, oldid}))
        return false;
    if (d_database.containsTable("call")) // dbv >= ~168?
      if (!d_database.exec("UPDATE call SET message_id = ? WHERE message_id = ?", {-1 * negative_id_tmp, oldid}))
        return false;
    if (d_database.tableContainsColumn(d_mms_table, "original_message_id")) // dbv >= ~197
      if (!d_database.exec("UPDATE " + d_mms_table + " SET original_message_id = ? WHERE original_message_id = ?", {-1 * negative_id_tmp, oldid}))
        return false;
    if (d_database.tableContainsColumn(d_mms_table, "latest_revision_id")) // dbv >= ~197
      if (!d_database.exec("UPDATE " + d_mms_table + " SET latest_revision_id = ? WHERE latest_revision_id = ?", {-1 * negative_id_tmp, oldid}))
        return false;
  }

  // now make all id's positive again
  if (!d_database.exec("UPDATE " + d_mms_table + " SET _id = _id * -1 WHERE _id < 0") ||
      !d_database.exec("UPDATE part SET mid = mid * -1 WHERE mid < 0") ||
      !d_database.exec("UPDATE group_receipts SET mms_id = mms_id * -1 WHERE mms_id < 0"))
    return false;
  if (d_database.containsTable("mention"))
    if (!d_database.exec("UPDATE mention SET message_id = message_id * -1 WHERE message_id < 0"))
      return false;
  if (d_database.containsTable("msl_message"))
    if (!d_database.exec("UPDATE msl_message SET message_id = message_id * -1 WHERE message_id < 0"s + (d_database.tableContainsColumn("msl_message", "is_mms") ? " AND is_mms IS 1" : "")))
      return false;
  if (d_database.containsTable("reaction")) // dbv >= 121
    if (!d_database.exec("UPDATE reaction SET message_id = message_id * -1 WHERE message_id < 0"s + (d_database.tableContainsColumn("reaction", "is_mms") ? " AND is_mms IS 1" : "")))
      return false;
  if (d_database.containsTable("story_sends"))
    if (!d_database.exec("UPDATE story_sends SET message_id = message_id * -1 WHERE message_id < 0"))
      return false;
  if (d_database.containsTable("call")) // dbv >= ~168?
    if (!d_database.exec("UPDATE call SET message_id = message_id * -1 WHERE message_id < 0"))
      return false;
  if (d_database.tableContainsColumn(d_mms_table, "original_message_id")) // dbv >= ~197
    if (!d_database.exec("UPDATE " + d_mms_table + " SET original_message_id = original_message_id * -1 WHERE original_message_id < 0"))
      return false;
  if (d_database.tableContainsColumn(d_mms_table, "latest_revision_id")) // dbv >= ~197
    if (!d_database.exec("UPDATE " + d_mms_table + " SET latest_revision_id = latest_revision_id * -1 WHERE latest_revision_id < 0"))
      return false;

  // SAME FOR SMS
  if (d_database.containsTable("sms")) // removed in 168
  {
    if (!d_database.exec("SELECT _id FROM sms ORDER BY " + d_sms_date_received + " ASC", &res))
      return false;

    negative_id_tmp = 0;
    for (uint i = 0; i < res.rows(); ++i)
    {
      long long int oldid = res.getValueAs<long long int>(i, 0);
    ++negative_id_tmp;
    if (!d_database.exec("UPDATE sms SET _id = ? WHERE _id = ?", {-1 * negative_id_tmp, oldid}))
      return false;
    if (d_database.containsTable("msl_message"))
      if (!d_database.exec("UPDATE msl_message SET message_id = ? WHERE message_id = ?"s + (d_database.tableContainsColumn("msl_message", "is_mms") ? " AND is_mms IS NOT 1" : ""), {-1 * negative_id_tmp, oldid}))
        return false;
    if (d_database.containsTable("reaction")) // dbv >= 121
      if (!d_database.exec("UPDATE reaction SET message_id = ? WHERE message_id = ?"s + (d_database.tableContainsColumn("reaction", "is_mms") ? " AND is_mms IS NOT 1" : ""), {-1 * negative_id_tmp, oldid}))
        return false;
    }

    if (!d_database.exec("UPDATE sms SET _id = _id * -1 WHERE _id < 0"))
      return false;
    if (d_database.containsTable("msl_message"))
      if (!d_database.exec("UPDATE msl_message SET message_id = message_id * -1 WHERE message_id < 0"s + (d_database.tableContainsColumn("msl_message", "is_mms") ? " AND is_mms IS NOT 1" : "")))
        return false;
    if (d_database.containsTable("reaction")) // dbv >= 121
      if (!d_database.exec("UPDATE reaction SET message_id = message_id * -1 WHERE message_id < 0"s + (d_database.tableContainsColumn("reaction", "is_mms") ? " AND is_mms IS NOT 1" : "")))
        return false;
  }
  return true;
}
