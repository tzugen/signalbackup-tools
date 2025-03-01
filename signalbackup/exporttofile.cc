/*
  Copyright (C) 2019-2023  Selwin van Dijk

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

bool SignalBackup::exportBackupToFile(std::string const &filename, std::string const &passphrase, bool overwrite, bool keepattachmentdatainmemory)
{
  std::cout << std::endl << "Exporting backup to '" << filename << "'" << std::endl;

  std::string newpw = passphrase;
  if (newpw == std::string())
    newpw = d_passphrase;
  if (newpw == std::string())
  {
    std::cout << "Need password to create encrypted backup file." << std::endl;
    return false;
  }

  if (!overwrite && bepaald::fileOrDirExists(filename))
  {
    std::cout << "File " << filename << " exists, use --overwrite to overwrite" << std::endl;
    return false;
  }

  if (!d_headerframe || !d_fe.init(newpw, d_headerframe->salt(), d_headerframe->salt_length(), d_headerframe->iv(), d_headerframe->iv_length(), d_headerframe->version(), d_verbose))
  {
    std::cout << "Error initializing FileEncryptor" << std::endl;
    return false;
  }

  std::ofstream outputfile(filename, std::ios_base::binary);

  // HEADER // Note: HeaderFrame is not encrypted.
  std::cout << "Writing HeaderFrame..." << std::endl;
  if (!d_headerframe)
  {
    std::cout << "Error: HeaderFrame not found" << std::endl;
    return false;
  }
  std::pair<unsigned char *, uint64_t> framedata = d_headerframe->getData();
  if (!framedata.first)
  {
    std::cout << "Error getting HeaderFrame data" << std::endl;
    return false;
  }
  bool writeok = writeFrameDataToFile(outputfile, framedata);
  delete[] framedata.first;
  if (!writeok)
    return false;

  // VERSION
  std::cout << "Writing DatabaseVersionFrame..." << std::endl;
  if (!d_databaseversionframe)
  {
    std::cout << "Error: DataBaseVersionFrame not found" << std::endl;
    return false;
  }
  if (!writeEncryptedFrame(outputfile, d_databaseversionframe.get()))
    return false;

  // SQL DATABASE + ATTACHMENTS
  std::cout << "Writing SqlStatementFrame(s)..." << std::endl;

  // get and write schema
  std::string q("SELECT sql, name, type FROM sqlite_master");
  SqliteDB::QueryResults results;
  d_database.exec(q, &results);
  std::vector<std::string> tables;

  for (uint i = 0; i < results.rows(); ++i)
  {
    if (!results.isNull(i, 0))
    {
      if (results.valueHasType<std::string>(i, 1) &&
          (results.getValueAs<std::string>(i, 1) != "sms_fts" &&
           STRING_STARTS_WITH(results.getValueAs<std::string>(i, 1), "sms_fts")))
        continue;//std::cout << "Skipping " << results[i][1].second << " because it is sms_ftssecrettable" << std::endl;

      if (results.valueHasType<std::string>(i, 1) &&
          (results.getValueAs<std::string>(i, 1) != d_mms_table + "_fts" &&
           STRING_STARTS_WITH(results.getValueAs<std::string>(i, 1), d_mms_table + "_fts")))
        continue;//std::cout << "Skipping " << results[i][1].second << " because it is mms_ftssecrettable" << std::endl;

      if (results.valueHasType<std::string>(i, 1) &&
          (results.getValueAs<std::string>(i, 1) != "emoji_search" &&
           STRING_STARTS_WITH(results.getValueAs<std::string>(i, 1), "emoji_search")))
        continue;//std::cout << "Skipping " << results[i][1].second << " because it is emoji_search_ftssecrettable" << std::endl;

      if (results.valueHasType<std::string>(i, 1) &&
          STRING_STARTS_WITH(results.getValueAs<std::string>(i, 1), "sqlite_"))
      {
        // this is normally skipped, but for testing purposes we won't skip if it was found in input
#ifdef BUILT_FOR_TESTING
        if (d_found_sqlite_sequence_in_backup)
          ;
        else
#endif
          continue;
      }

      if (results.valueHasType<std::string>(i, 2) && results.getValueAs<std::string>(i, 2) == "table")
        tables.emplace_back(results.getValueAs<std::string>(i, 1));

      SqlStatementFrame newframe;
      newframe.setStatementField(results.getValueAs<std::string>(i, 0));

      //std::cout << "Writing SqlStatementFrame..." << std::endl;
      if (!writeEncryptedFrame(outputfile, &newframe))
        return false;
    }
  }

  // write contents of tables
  for (std::string const &table : tables)
  {
    if (table == "signed_prekeys" ||
        table == "one_time_prekeys" ||
        table == "sessions" ||
        //table == "job_spec" ||           // this is in the official export. But it makes testing more difficult. it
        //table == "constraint_spec" ||    // should be ok to export these (if present in source), since we are only
        //table == "dependency_spec" ||    // dealing with exported backups (not from live installations) -> they should
        //table == "emoji_search" ||       // have been excluded + the official import should be able to deal with them
        //table == "sender_keys" ||
        //table == "sender_key_shared" ||
        //table == "pending_retry_receipts" ||
        //table == "avatar_picker" ||
        //table == "" ||
        STRING_STARTS_WITH(table, "sms_fts") ||
        STRING_STARTS_WITH(table, d_mms_table + "_fts") ||
        STRING_STARTS_WITH(table, "sqlite_"))
      continue;

    d_database.exec("SELECT * FROM " + table, &results);

    if (!d_showprogress)
      std::cout << "  Dealing with table '" << table << "'... " << std::flush;

    for (uint i = 0; i < results.rows(); ++i)
    {
      if (d_showprogress)
        std::cout << "\33[2K\r  Dealing with table '" << table << "'... " << i + 1 << "/" << results.rows() << " entries..." << std::flush;

      SqlStatementFrame newframe = buildSqlStatementFrame(table, results.row(i));

      //std::cout << "Writing SqlStatementFrame..." << std::endl;
      if (!writeEncryptedFrame(outputfile, &newframe))
        return false;

      if (table == "part") // find corresponding attachment
      {
        uint64_t rowid = 0, uniqueid = 0;
        for (uint j = 0; j < results.columns(); ++j)
        {
          if (results.header(j) == "_id" && results.valueHasType<long long int>(i, j))
          {
            rowid = results.getValueAs<long long int>(i, j);
            if (rowid && uniqueid)
              break;
          }
          else if (results.header(j) == "unique_id" && results.valueHasType<long long int>(i, j))
          {
           //std::cout << "UNIQUEID: " << std::any_cast<long long int>(results[i][j].second) << std::endl;
            uniqueid = results.getValueAs<long long int>(i, j);
            if (rowid && uniqueid)
              break;
          }
        }
        auto attachment = d_attachments.find({rowid, uniqueid});
        if (attachment != d_attachments.end())
        {
          if (!writeEncryptedFrame(outputfile, attachment->second.get()))
            return false;
          if (!keepattachmentdatainmemory)
          {
            MEMINFO("BEFORE DROPPING ATTACHMENT DATA");
            attachment->second.get()->clearData();
            MEMINFO("AFTER DROPPING ATTACHMENT DATA");
          }
        }
        else
        {
          if (!missingAttachmentExpected(rowid, uniqueid))
          {
            std::cout << "Warning: attachment data not found (rowid: " << rowid << ", uniqueid: " << uniqueid << ")" << std::endl;
            if (d_showprogress)
              std::cout << "\33[2K\r  Dealing with table '" << table << "'... " << i + 1 << "/" << results.rows() << " entries..." << std::flush;
          }
        }
      }
      else if (table == "sticker") // find corresponding sticker
      {
        uint64_t rowid = 0;
        for (uint j = 0; j < results.columns(); ++j)
          if (results.header(j) == "_id" && results.valueHasType<long long int>(i, j))
          {
            rowid = results.getValueAs<long long int>(i, j);
            break;
          }
        auto sticker = d_stickers.find(rowid);
        if (sticker != d_stickers.end())
        {
          if (!writeEncryptedFrame(outputfile, sticker->second.get()))
            return false;
          if (!keepattachmentdatainmemory)
            sticker->second.get()->clearData();
        }
        else
        {
          std::cout << "Warning: sticker data not found (rowid: " << rowid << ")" << std::endl;
          if (d_showprogress)
            std::cout << "\33[2K\r  Dealing with table '" << table << "'... " << i + 1 << "/" << results.rows() << " entries..." << std::flush;
        }
      }
    }
    if (results.rows())
        std::cout << "done" << std::endl;
    else
      std::cout << "  Dealing with table '" << table << "'... 0/0 entries..." << std::endl;
  }

  std::cout << "Writing SharedPrefFrame(s)..." << std::endl;
  // SHAREDPREFS
  for (uint i = 0; i < d_sharedpreferenceframes.size(); ++i)
    if (!writeEncryptedFrame(outputfile, d_sharedpreferenceframes[i].get()))
      return false;

  std::cout << "Writing KeyValueFrame(s)..." << std::endl;
  // KEYVALUES
  for (uint i = 0; i < d_keyvalueframes.size(); ++i)
    if (!writeEncryptedFrame(outputfile, d_keyvalueframes[i].get()))
      return false;

  // AVATAR
  std::cout << "Writing Avatars..." << std::endl;
  for (auto const &a : d_avatars)
  {

    if (d_verbose && !a.second.get()) [[unlikely]]
    {
      std::cout << "ASKED TO WRITE NULLPTR-AVATAR. THIS SHOULD BE AN ERROR" << std::endl;
      std::cout << "BUT I'M PRETENDING IT DIDN'T HAPPEN TO FIND THE CAUSE OF IT" << std::endl;
      std::cout << "THE PROGRAM WILL LIKELY CRASH NOW..." << std::endl;
    }

    if (!writeEncryptedFrame(outputfile, a.second.get()))
      return false;
  }

  // END
  std::cout << "Writing EndFrame..." << std::endl;
  if (!d_endframe)
  {
    std::cout << "Error: EndFrame not found" << std::endl;
    return false;
  }
  if (!writeEncryptedFrame(outputfile, d_endframe.get()))
    return false;

  outputfile.flush();

  std::cout << "Done! Wrote " << outputfile.tellp() << " bytes." << std::endl;
  return true;
}
