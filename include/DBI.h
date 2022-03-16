/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, Internet APRS MySQL Injector                               **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
 **                    Daniel Robert Karrels                             **
 **                    Dynamic Networking Solutions                      **
 **                                                                      **
 ** This program is free software; you can redistribute it and/or modify **
 ** it under the terms of the GNU General Public License as published by **
 ** the Free Software Foundation; either version 1, or (at your option)  **
 ** any later version.                                                   **
 **                                                                      **
 ** This program is distributed in the hope that it will be useful,      **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of       **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        **
 ** GNU General Public License for more details.                         **
 **                                                                      **
 ** You should have received a copy of the GNU General Public License    **
 ** along with this program; if not, write to the Free Software          **
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            **
 **************************************************************************/

#ifndef APNSPUSHER_DBI_H
#define APNSPUSHER_DBI_H

#include <openframe/DBI.h>

namespace apnspusher {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

#define NULL_OPTIONPP(x, y) ( x->getString(y).length() > 0 ? mysqlpp::SQLTypeAdapter(x->getString(y)) : mysqlpp::SQLTypeAdapter(mysqlpp::null) )

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  class DBI_Apns : public openframe::DBI {
    public:
      DBI_Apns(const thread_id_t thread_id,
               const std::string &db,
               const std::string &host,
               const std::string &user,
               const std::string &pass);
      virtual ~DBI_Apns();

      void prepare_queries();

      resultSizeType getApnsRegisterByCallsign(const std::string &callsign,
                                               resultType &res);
      simpleResultSizeType setApnsPush(const std::string &id,
                                       const std::string &message);

    protected:
    private:
  }; // class DBI_Apns

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/

} // namespace apnspusher
#endif
