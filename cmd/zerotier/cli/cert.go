/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package cli

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"zerotier/pkg/zerotier"
)


func Cert(basePath, authToken string, args []string, jsonOutput bool) int {
	if len(args) < 1 {
		Help()
		return 1
	}

	switch args[0] {

	case "newsid":
		if len(args) > 2 {
			Help()
			return 1
		}
		uniqueId, uniqueIdPrivate, err := zerotier.NewCertificateSubjectUniqueId(zerotier.CertificateUniqueIdTypeNistP384)
		if err != nil {
			fmt.Printf("ERROR: unable to create unique ID and private key: %s\n", err.Error())
			return 1
		}
		sec, err := json.MarshalIndent(&zerotier.CertificateSubjectUniqueIDSecret{UniqueID: uniqueId, UniqueIDSecret: uniqueIdPrivate}, "", "  ")
		if err != nil {
			fmt.Printf("ERROR: unable to create unique ID and private key: %s\n", err.Error())
			return 1
		}
		if len(args) == 1 {
			fmt.Println(string(sec))
		} else {
			_ = ioutil.WriteFile(args[1], sec, 0600)
		}

	case "newcsr":
		if len(args) < 3 {
			Help()
			return 1
		}
		var cs zerotier.CertificateSubject
		csb, err := ioutil.ReadFile(args[1])
		if err != nil {
			fmt.Printf("ERROR: unable to read subject from %s: %s\n", args[1], err.Error())
			return 1
		}
		err = json.Unmarshal(csb, &cs)
		if err != nil {
			fmt.Printf("ERROR: unable to read subject from %s: %s\n", args[1], err.Error())
			return 1
		}
		var subj zerotier.CertificateSubjectUniqueIDSecret
		subjb, err := ioutil.ReadFile(args[2])
		if err != nil {
			fmt.Printf("ERROR: unable to read unique ID secret from %s: %s\n", args[2], err.Error())
			return 1
		}
		err = json.Unmarshal(subjb, &subj)
		if err != nil {
			fmt.Printf("ERROR: unable to read unique ID secret from %s: %s\n", args[2], err.Error())
			return 1
		}
		csr, err := zerotier.NewCertificateCSR(&cs, subj.UniqueID, subj.UniqueIDSecret)
		if err != nil {
			fmt.Printf("ERROR: problem creating CSR: %s\n", err.Error())
			return 1
		}
		if len(args) == 3 {
			_, _ = os.Stdout.Write(csr)
		} else {
			_ = ioutil.WriteFile(args[3], csr, 0644)
		}

	case "sign":

	case "verify":

	case "show":
		if len(args) != 1 {
			Help()
			return 1
		}

	case "import":

	case "restore":

	case "export":

	case "delete":

	}

	return 0
}
