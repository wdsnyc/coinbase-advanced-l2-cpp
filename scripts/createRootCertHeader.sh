#!/bin/bash

# run update-ca-trust as root

FILENAME=tls-ca-bundle-pem.h

rsync -aiv /etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem $FILENAME
chmod 666 $FILENAME

sed -i -r \
    -e 's/^(.)/"\1/' \
    -e 's/(.)$/\1\\n"/' \
    -e '/^"#/i cert +=' \
    -e '/END CERTIFICATE/a "\\n";' \
    $FILENAME

sed -i -r \
    -e '1 i #ifndef CA_BUNDLE_TRUST_H\n#define CA_BUNDLE_TRUST_H\n\n#include <boost/asio/ssl.hpp>\n\n\/\/start1\n' \
    $FILENAME

sed -i -r \
    -e '/start1/a namespace ssl = boost::asio::ssl;\n\nnamespace coinbase {\n\n/\/start2\n' \
    $FILENAME

sed -i -r \
    -e '/start2/a void load_root_certificates(ssl::context& ctx, boost::system::error_code& ec)\n{\n    std::string cert;\n\/\/start3' \
    $FILENAME

sed -i -e '/\/\/start[123]/d' $FILENAME

echo ""                                                                                     >> $FILENAME
echo "   ctx.add_certificate_authority("                                                    >> $FILENAME
echo "        boost::asio::buffer(cert.data(), cert.size()), ec);"                          >> $FILENAME
echo "    if(ec)"                                                                           >> $FILENAME
echo "        return;"                                                                      >> $FILENAME
echo "}"                                                                                    >> $FILENAME
echo "} // namespace coinbase"                                                              >> $FILENAME
echo ""                                                                                     >> $FILENAME
echo "inline void load_root_certificates(ssl::context& ctx, boost::system::error_code& ec)" >> $FILENAME
echo "{"                                                                                    >> $FILENAME
echo "   coinbase::load_root_certificates(ctx, ec);"                                        >> $FILENAME
echo "}"                                                                                    >> $FILENAME
echo ""                                                                                     >> $FILENAME
echo "inline void load_root_certificates(ssl::context& ctx)"                                >> $FILENAME
echo "{"                                                                                    >> $FILENAME
echo "    boost::system::error_code ec;"                                                    >> $FILENAME
echo "    coinbase::load_root_certificates(ctx, ec);"                                       >> $FILENAME
echo "    if(ec)"                                                                           >> $FILENAME
echo "        throw boost::system::system_error{ec};"                                       >> $FILENAME
echo "}"                                                                                    >> $FILENAME
echo ""                                                                                     >> $FILENAME
echo "#endif"                                                                               >> $FILENAME
