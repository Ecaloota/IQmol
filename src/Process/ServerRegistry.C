/*******************************************************************************
       
  Copyright (C) 2011-2015 Andrew Gilbert
           
  This file is part of IQmol, a free molecular visualization program. See
  <http://iqmol.org> for more details.
       
  IQmol is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  IQmol is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.
      
  You should have received a copy of the GNU General Public License along
  with IQmol.  If not, see <http://www.gnu.org/licenses/>.  
   
********************************************************************************/

#include "ServerRegistry.h"
#include "Preferences.h"
#include "Server.h"
#include "QMsgBox.h"
#include "Exception.h"
#include "YamlNode.h"
#include "ParseFile.h"
#include <QDir>


namespace IQmol {
namespace Process {

ServerRegistry* ServerRegistry::s_instance = 0;
QList<Server*>  ServerRegistry::s_servers;
QList<Server*>  ServerRegistry::s_deletedServers;


ServerRegistry& ServerRegistry::instance() 
{
   if (s_instance == 0) {
      s_instance = new ServerRegistry();

      loadFromPreferences();
      atexit(ServerRegistry::destroy);
   }
   return *s_instance;
}


QStringList ServerRegistry::availableServers() const
{
   QStringList list;
   QList<Server*>::iterator iter;
   for (iter = s_servers.begin(); iter != s_servers.end(); ++iter) {
       list.append((*iter)->name());
   }
   return list;
}


Server* ServerRegistry::addServer(ServerConfiguration& config)
{
   QString name(config.value(ServerConfiguration::ServerName));
   int count(0);

   while (find(name)) {
      ++count;
      name = config.value(ServerConfiguration::ServerName) + "_" + QString::number(count);
   }

   config.setValue(ServerConfiguration::ServerName, name);

   Server* server(new Server(config));
   s_servers.append(server);
   save();

   return server;
}


void ServerRegistry::closeAllConnections()
{
   QList<Server*>::iterator iter;
   for (iter = s_servers.begin(); iter != s_servers.end(); ++iter) {
       (*iter)->closeConnection();
   }
}


void ServerRegistry::connectServers(QStringList const& servers)
{
   QStringList::const_iterator iter;
   for (iter = servers.begin(); iter != servers.end(); ++iter) {
       Server* server(find(*iter));
       if (server) {
          server->open();
       }
   }
}


Server* ServerRegistry::find(QString const& serverName)
{
   int index(indexOf(serverName));
   if (index >= 0) return s_servers[index];
   return 0;
}


void ServerRegistry::remove(QString const& serverName)
{
   int index(indexOf(serverName));
   if (index >= 0) s_deletedServers.append(s_servers.takeAt(index));
   save();
}


void ServerRegistry::remove(Server* server)
{
   int index(s_servers.indexOf(server));
   if (index >= 0) s_deletedServers.append(s_servers.takeAt(index));
   save();
}


void ServerRegistry::moveUp(QString const& serverName)
{
   int index(indexOf(serverName));
   if (index > 0) s_servers.swap(index, index-1);
   save();
}


void ServerRegistry::moveDown(QString const& serverName)
{
   int index(indexOf(serverName));
   if (index < s_servers.size() -1) s_servers.swap(index, index+1);
   save();
}


int ServerRegistry::indexOf(QString const& serverName) 
{
   for (int i = 0; i < s_servers.size(); ++i) {
      if (s_servers[i]->name() == serverName) return i;
   }
   return -1;
}


void ServerRegistry::destroy() 
{
   QList<Server*>::iterator iter;
   for (iter = s_servers.begin(); iter != s_servers.end(); ++iter) {
       delete (*iter);
   }
   for (iter = s_deletedServers.begin(); iter != s_deletedServers.end(); ++iter) {
       delete (*iter);
   }
}


void ServerRegistry::loadFromPreferences()
{
   QVariantList list(Preferences::ServerConfigurationList());
   QVariantList::iterator iter;

   try {
      for (iter = list.begin(); iter != list.end(); ++iter) {
          s_servers.append(new Server(ServerConfiguration(*iter)));
      }


      // Look for default servers in the share directory
      if (s_servers.isEmpty()) {
         QDir dir(Preferences::ServerDirectory());
         qDebug() << "Server Directory set to: " << dir.absolutePath();
         if (dir.exists()) {
            QStringList filters;
            filters << "*.cfg";
            QStringList listing(dir.entryList(filters, QDir::Files));
         
            QStringList::iterator iter;
            for (iter = listing.begin(); iter != listing.end(); ++iter) {
                QFileInfo info(dir, *iter);
                qDebug() << "Reading server configutation from: " << info.absoluteFilePath();
                ServerConfiguration serverConfiguration;
                if (loadFromFile(info.absoluteFilePath(), serverConfiguration)) {
                   addServer(serverConfiguration);
                   //s_servers.append(new Server(serverConfiguration));
                }
            }
         }
      }

      // Finally, if there are no servers, add the eefault iqmol.q-chem.com server
      if (s_servers.isEmpty()) {
         qDebug() << "Appending Q-Chem server";
         s_servers.append(new Server(ServerConfiguration()));
      }

   } catch (Exception& ex) {
      QString msg("Problem loading servers from Preferences file:\n");
      msg += ex.what();
      QMsgBox::warning(0, "IQmol", msg);
   }
}


void ServerRegistry::saveToPreferences()
{
   QVariantList list;
   QList<Server*>::const_iterator iter;
   for (iter = s_servers.begin(); iter != s_servers.end(); ++iter) {
       list.append((*iter)->configuration().toQVariant());
   }
   Preferences::ServerConfigurationList(list);
}


bool ServerRegistry::loadFromFile(QString const& filePath, ServerConfiguration& serverConfig)
{
   bool ok(false);

   QFile file(filePath);
   if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QLOG_ERROR() << "Server configuraiton file does not exist" << filePath;
      return ok;
   }   

   Parser::ParseFile parser(filePath);
   parser.start();
   parser.wait();

   QStringList errors(parser.errors());
   if (!errors.isEmpty()) {
      QLOG_ERROR() << errors.join("\n");
   }   

   Data::Bank& bank(parser.data());
   QList<Data::YamlNode*> yaml(bank.findData<Data::YamlNode>());
   if (yaml.first()) {
      yaml.first()->dump();
      serverConfig = ServerConfiguration(*(yaml.first()));
      ok = true;
   }   

   return ok;
}


} } // end namespace IQmol::Process
