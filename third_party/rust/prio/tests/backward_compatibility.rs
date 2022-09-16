use prio::{test_vector::Priov2TestVector, util::reconstruct_shares};

#[test]
fn priov2_backward_compatibility() {
    let test_vector: Priov2TestVector =
        serde_json::from_str(include_str!("test_vectors/fieldpriov2.json")).unwrap();

    let mut server1 = test_vector.server_1().unwrap();
    let mut server2 = test_vector.server_2().unwrap();

    for (server_1_share, server_2_share) in test_vector
        .server_1_shares
        .iter()
        .zip(&test_vector.server_2_shares)
    {
        let eval_at = server1.choose_eval_at();

        let v1 = server1
            .generate_verification_message(eval_at, server_1_share)
            .unwrap();
        let v2 = server2
            .generate_verification_message(eval_at, server_2_share)
            .unwrap();

        assert!(server1.aggregate(server_1_share, &v1, &v2).unwrap());
        assert!(server2.aggregate(server_2_share, &v1, &v2).unwrap());
    }

    let reconstructed = reconstruct_shares(server1.total_shares(), server2.total_shares()).unwrap();
    assert_eq!(reconstructed, test_vector.reference_sum);
}
