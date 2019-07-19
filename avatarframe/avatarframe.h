/*
    Copyright (C) 2019  Selwin van Dijk

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

#ifndef AVATARFRAME_H_
#define AVATARFRAME_H_

#include <cstring>

#include "../framewithattachment/framewithattachment.h"

class AvatarFrame : public FrameWithAttachment
{
  enum FIELD
  {
    NAME = 1, // string
    LENGTH = 2 // uint32
  };

  static Registrar s_registrar;
 public:
  inline AvatarFrame(unsigned char *bytes, size_t length, uint64_t count);
  // inline AvatarFrame(AvatarFrame &&other);
  // inline AvatarFrame &operator=(AvatarFrame &&other);
  // inline AvatarFrame(AvatarFrame const &other) = delete;
  // inline AvatarFrame &operator=(AvatarFrame const &other) = delete;
  inline virtual ~AvatarFrame() = default;
  inline static BackupFrame *create(unsigned char *bytes, size_t length, uint64_t count);
  inline virtual void printInfo() const override;
  inline virtual FRAMETYPE frameType() const override;
  inline uint32_t length() const;
  inline virtual uint32_t attachmentSize() const override;
  inline std::string name() const;
  inline std::pair<unsigned char *, uint64_t> getData() const;
  inline virtual bool validate() const override;
 private:
  inline uint64_t dataSize() const;
};

inline AvatarFrame::AvatarFrame(unsigned char *bytes, size_t length, uint64_t count)
  :
  FrameWithAttachment(bytes, length, count)
{}

// inline AvatarFrame::AvatarFrame(AvatarFrame &&other)
//   :
//   FrameWithAttachment(std::move(other))
// {}

// inline AvatarFrame &AvatarFrame::operator=(AvatarFrame &&other)
// {
//   if (this != &other)
//   {
//     FrameWithAttachment::operator=(std::move(other));
//   }
//   return *this;
// }

// inline AvatarFrame::~AvatarFrame()
// {}

inline BackupFrame *AvatarFrame::create(unsigned char *bytes, size_t length, uint64_t count) // static
{
  return new AvatarFrame(bytes, length, count);
}

inline void AvatarFrame::printInfo() const // virtual override
{
  std::cout << "Frame number: " << d_count << std::endl;
  std::cout << "        Type: AVATAR" << std::endl;
  for (auto const &p : d_framedata)
  {
    if (std::get<0>(p) == FIELD::NAME)
      std::cout << "         - name  : " << bepaald::bytesToString(std::get<1>(p), std::get<2>(p)) << " (" << std::get<2>(p) << " bytes)" << std::endl;
    else if (std::get<0>(p) == FIELD::LENGTH)
      std::cout << "         - length: " << bytesToUint32(std::get<1>(p), std::get<2>(p)) << " (" << std::get<2>(p) << " bytes)" << std::endl;
  }
  if (d_attachmentdata)
  {
    uint32_t size = length();
    if (size < 25)
      std::cout << "         - attachment      : " << bepaald::bytesToHexString(d_attachmentdata, size) << std::endl;
    else
      std::cout << "         - attachment      : " << bepaald::bytesToHexString(d_attachmentdata, 25) << " ... (" << size << " bytes total)" << std::endl;
  }
}

inline FRAMETYPE AvatarFrame::frameType() const // virtual override
{
  return FRAMETYPE::AVATAR;
}

inline uint32_t AvatarFrame::length() const
{
  if (!d_attachmentdata_size)
    for (auto const &p : d_framedata)
      if (std::get<0>(p) == FIELD::LENGTH)
        return  bytesToUint32(std::get<1>(p), std::get<2>(p));
  return d_attachmentdata_size;
}

inline uint32_t AvatarFrame::attachmentSize() const // virtual override
{
  return length();
}

inline std::string AvatarFrame::name() const
{
  for (auto const &p : d_framedata)
    if (std::get<0>(p) == FIELD::NAME)
      return bepaald::bytesToString(std::get<1>(p), std::get<2>(p));
  return std::string();
}

inline uint64_t AvatarFrame::dataSize() const
{
  uint64_t size = 0;

  for (auto const &fd : d_framedata)
  {
    switch (std::get<0>(fd))
    {
    case FIELD::NAME:
      {
        uint64_t stringsize = std::get<2>(fd);
        size += varIntSize(stringsize);
        size += stringsize + 1; // +1 for fieldtype + wiretype
        break;
      }
    case FIELD::LENGTH:
      {
        uint64_t value = bytesToUint64(std::get<1>(fd), std::get<2>(fd));
        size += varIntSize(value);
        size += 1; // for fieldtype + wiretype
        break;
      }
    }
  }

  // for size of this entire frame.
  size += varIntSize(size);
  return ++size;  // for frametype and wiretype
}

inline std::pair<unsigned char *, uint64_t> AvatarFrame::getData() const
{
  uint64_t size = dataSize();
  unsigned char *data = new unsigned char[size];
  uint64_t datapos = 0;

  datapos += setFieldAndWire(FRAMETYPE::AVATAR, WIRETYPE::LENGTHDELIM, data + datapos);
  datapos += setFrameSize(size, data + datapos);

  for (auto const &fd : d_framedata)
  {
    switch (std::get<0>(fd))
    {
    case FIELD::NAME:
      datapos += putLengthDelimType(fd, data + datapos);
      break;
    case FIELD::LENGTH:
      datapos += putVarIntType(fd, data + datapos);
      break;
    }
  }
  return {data, size};
}

inline bool AvatarFrame::validate() const
{
  if (d_framedata.empty())
    return false;

  for (auto const &p : d_framedata)
  {
    if (std::get<0>(p) != FIELD::NAME &&
        std::get<0>(p) != FIELD::LENGTH)
      return false;
  }
  return true;
}

#endif